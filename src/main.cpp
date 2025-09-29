// Copyright (C) 2025  Instellate
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <iostream>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <ranges>
#include <chrono>

#include <alpm.h>
#include <pwd.h>
#include <set>
#include <curl/curl.h>
#include <libnotify/notify.h>

extern "C" {
#include <pacutils.h>
}

namespace fs = std::filesystem;
namespace chrono = std::chrono;

alpm_pkg_t *getSyncPkg(alpm_handle_t *handle, const char *name) {
    const alpm_list_t *list = alpm_get_syncdbs(handle);

    while (list) {
        auto *db = static_cast<alpm_db_t *>(list->data);
        alpm_pkg_t *pkg = alpm_db_get_pkg(db, name);
        if (pkg) {
            return pkg;
        }

        list = alpm_list_next(list);
    }

    return nullptr;
}

int main() {
    pu_config_t *config = pu_config_new();
    pu_ui_config_load_sysroot(config, "/etc/pacman.conf", nullptr);

    std::unordered_map<std::string, std::vector<std::string> > repos;

    alpm_list_t *repoList = config->repos;
    while (repoList) {
        const auto repo = static_cast<pu_repo_t *>(repoList->data);

        std::vector<std::string> servers;
        alpm_list_t *serverList = repo->servers;
        while (serverList) {
            servers.emplace_back(static_cast<char *>(serverList->data));
            serverList = alpm_list_next(serverList);
        }

        repos.emplace(std::string{repo->name}, std::move(servers));
        repoList = alpm_list_next(repoList);
    }

    passwd *pwuid = getpwuid(getuid());
    if (!pwuid) {
        std::cerr << "Couldn't get pwuid\n";
        return -1;
    }

    fs::path userDir{pwuid->pw_dir};
    fs::path configDir = userDir / ".cache/pacgrade";
    std::cout << "Configuration directory is: " << configDir << '\n';

    if (!fs::exists(configDir)) {
        std::cout << "Configuration directory doesn't exist, creating.\n";
        fs::create_directories(configDir);
    }

    fs::path fakeDbDir = configDir / "db";
    if (!fs::exists(fakeDbDir)) {
        std::cout << "Emulated database dir does not exist, creating\n";
        fs::create_directories(fakeDbDir);
    }

    fs::path localDir = fakeDbDir / "local";
    if (!fs::exists(localDir)) {
        std::cout << "Local directory doesn't exist. Symlinking against /var/lib/pacman/local\n";
        fs::create_directory_symlink(fs::path{"/var/lib/pacman/local"}, localDir);
    }

    fs::path syncDir = fakeDbDir / "sync";
    if (!fs::exists(syncDir)) {
        std::cout << "Creating sync dir\n";
        fs::create_directories(syncDir);
    }

    for (const auto &[repo, servers]: repos) {
        CURL *curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Couldn't initialize curl\n";
            return -1;
        }

        fs::path repoDb = syncDir / (repo + ".db");

        if (servers.empty()) {
            std::cerr << "There are no available servers for repository " << repo << '\n';
            continue;
        }

        std::string server = servers[0];
        if (!server.ends_with("/")) {
            server += "/";
        }
        server += repo + ".db";

        curl_easy_setopt(curl, CURLOPT_URL, server.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, CURLFOLLOW_ALL);

        curl_easy_perform(curl);

        curl_header *lastModifiedHeader = nullptr;
        curl_easy_header(curl, "last-modified", 0, CURLH_HEADER, -1, &lastModifiedHeader);

        tm time{};
        strptime(lastModifiedHeader->value, "%a, %d %b %Y %H:%M:%S GMT", &time);
        time_t lastModifiedRemote = timegm(&time);

        curl_easy_cleanup(curl);

        if (fs::exists(repoDb)) {
            auto lastWriteTime = chrono::clock_cast<chrono::system_clock>(fs::last_write_time(repoDb));
            auto lastWriteLocal = chrono::duration_cast<chrono::seconds>(lastWriteTime.time_since_epoch()).count();

            if (lastModifiedRemote < lastWriteLocal) {
                std::cout << "Repository database " << repo << " is up to date, skipping.\n";
                continue;
            }
        }

        curl = curl_easy_init();

        FILE *db = fopen(repoDb.c_str(), "w+");

        curl_easy_setopt(curl, CURLOPT_URL, server.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void *>(db));
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, CURLFOLLOW_ALL);

        CURLcode resCode = curl_easy_perform(curl);
        if (resCode != CURLE_OK) {
            std::cout << "Couldn't get database for repository " << repo << '\n';
        } else {
            std::cout << "Downloaded database file for repository " << repo << '\n';
        }

        curl_easy_cleanup(curl);
        fclose(db);
    }

    std::cout << "Initializing alpm handle...\n";

    alpm_errno_t handleInitError{};
    alpm_handle_t *handle = alpm_initialize("/", fakeDbDir.c_str(), &handleInitError);
    if (!handle) {
        std::cerr << "Couldn't initialize alpm handle: " << alpm_strerror(handleInitError) << '\n';
        return -1;
    }


    std::cout << "Loading Sync databases into handle\n";
    for (const auto &repo: repos | std::views::keys) {
        alpm_db_t *db = alpm_register_syncdb(handle, repo.c_str(), 0);
        if (!db) {
            std::cerr << "Couldn't load sync database for repository " << repo << ": " << alpm_strerror(
                        alpm_errno(handle)) <<
                    '\n';
        } else {
            std::cout << "Loaded in sync database for repository " << repo << '\n';
        }
    }

    alpm_db_t *localDb = alpm_get_localdb(handle);
    if (!localDb) {
        std::cerr << "Couldn't get local database\n";
        return -1;
    }

    std::cout << "Looking for out of date packages\n";
    size_t amount = 0;

    alpm_list_t *pkgList = alpm_db_get_pkgcache(localDb);
    while (pkgList) {
        auto *pkg = static_cast<alpm_pkg_t *>(pkgList->data);
        auto pkgName = alpm_pkg_get_name(pkg);

        alpm_pkg_t *syncPkg = getSyncPkg(handle, pkgName);
        if (!syncPkg) {
            std::cerr << "Couldn't find sync database for package: " << pkgName << '\n';
            pkgList = alpm_list_next(pkgList);
            continue;
        }

        alpm_time_t localBuildDate = alpm_pkg_get_builddate(pkg);
        alpm_time_t syncBuildDate = alpm_pkg_get_builddate(syncPkg);
        const char *localVersion = alpm_pkg_get_version(pkg);
        const char *syncVersion = alpm_pkg_get_version(syncPkg);

        if (syncBuildDate > localBuildDate && strcmp(localVersion, syncVersion) != 0) {
            std::cout << "Package " << pkgName << " is most likely out of date\n";
            std::cout << "\tLocal version: " << localVersion << '\n';
            std::cout << "\tSync version: " << syncVersion << '\n';
            amount++;
        }

        pkgList = alpm_list_next(pkgList);
    }

    std::cout << "Found " << amount << " packages that are out of date\n";

    notify_init("Pacgrade");

    if (amount > 0) {
        std::stringstream ss;
        if (amount > 1) {
            ss << "Found " << amount << " packages that are out of date.\nRemember to upgrade!";
        } else {
            ss << "Found a package that is out of date.\nRemember to upgrade!";
        }

        std::string description = ss.str();

        NotifyNotification *notification =
                notify_notification_new("Packages out of date", description.c_str(), nullptr);
        notify_notification_show(notification, nullptr);
    }

    alpm_release(handle);
    pu_config_free(config);
    return 0;
}
