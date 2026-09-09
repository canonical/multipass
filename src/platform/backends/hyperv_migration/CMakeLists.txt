# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

add_library(hyperv_migration STATIC
  hyperv_disk_layout.cpp
  hyperv_migration_service.cpp
  hyperv_target_transaction.cpp)

target_include_directories(hyperv_migration
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(hyperv_migration
  PUBLIC
    Boost::json
    fmt::fmt-header-only
  PRIVATE
    hyperv_api_backend
    logger
    shared_win
    utils
    Qt6::Core)
