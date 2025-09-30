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

#pragma once

#include <iterator>
#include <stdexcept>

#include <alpm_list.h>

namespace pacgrade {
    template<typename T>
    class ALPMList {
        alpm_list_t *_list;

    public:
        class Iterator {
            alpm_list_t *_list;

            explicit Iterator(alpm_list_t *list) : _list(list) {
            }

            friend ALPMList;

        public:
            using difference_type = std::ptrdiff_t;
            using value_type = T;

            Iterator() : _list(nullptr) {
            }

            Iterator(const Iterator &) = default;

            Iterator &operator=(const Iterator &) = default;

            T operator*() const {
                if (!this->_list) {
                    throw std::out_of_range{"List is at the end"};
                }

                return static_cast<T>(this->_list->data);
            }

            Iterator &operator++() {
                if (this->_list) {
                    this->_list = alpm_list_next(this->_list);
                }

                return *this;
            }

            Iterator operator++(int) {
                auto tmp = *this;
                ++*this;
                return tmp;
            }

            bool operator==(const Iterator &it) const {
                return this->_list == it._list;
            }
        };

        explicit ALPMList(alpm_list_t *list) : _list(list) {
        }

        [[nodiscard]] size_t count() const {
            return alpm_list_count(this->_list);
        }

        [[nodiscard]] Iterator begin() const {
            return Iterator{this->_list};
        }

        [[nodiscard]] Iterator end() const {
            return Iterator{};
        }

        T operator[](const int index) const {
            if (index > count()) {
                throw std::out_of_range{"index"};
            }

            return alpm_list_nth(this->_list, index);
        }
    };

    static_assert(std::forward_iterator<ALPMList<char *>::Iterator>,
                  "ALPMList::Iterator does not implement forward iteration");
} // pacgrade
