# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

if(WIN32)
  add_library(hyperv_api_backend STATIC)

  target_sources(hyperv_api_backend PRIVATE
    hyperv_api_common.cpp 
    hcn/hyperv_hcn_api_wrapper.cpp
    hcs/hyperv_hcs_api_wrapper.cpp)

  target_link_libraries(hyperv_api_backend PRIVATE
    fmt
    utils)
endif()
