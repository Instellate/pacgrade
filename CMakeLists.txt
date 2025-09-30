cmake_minimum_required(VERSION 4.0)
project(pacgrade)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FindPkgConfig)

pkg_check_modules(libalpm REQUIRED libalpm)
pkg_check_modules(libnotify REQUIRED libnotify)

find_library(PACUTILS pacutils REQUIRED)
find_package(CURL COMPONENTS HTTP HTTPS REQUIRED)

add_executable(pacgrade
        src/main.cpp
        src/ALPMList.h)
target_link_libraries(pacgrade PRIVATE ${libalpm_LIBRARIES} ${PACUTILS} ${CURL_LIBRARIES} ${libnotify_LIBRARIES})
target_include_directories(pacgrade PRIVATE ${CURL_INCLUDE_DIRS} ${libalpm_INCLUDE_DIRS} ${libnotify_INCLUDE_DIRS})

install(TARGETS pacgrade)