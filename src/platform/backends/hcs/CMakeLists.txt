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

  include(CheckCXXSourceRuns)

  macro(check_pragma_lib LIB_NAME HEADER_NAME OUT_VAR)
    check_cxx_source_runs("
      #pragma comment(lib, \"${LIB_NAME}\")
      #define WIN32_LEAN_AND_MEAN
      #include <windows.h>
      #include <${HEADER_NAME}>
      int main(void){ return 0; }
    " ${OUT_VAR})
  endmacro()

  check_pragma_lib("computecore.lib"  "computecore.h" HAS_COMPUTECORE)
  check_pragma_lib("computenetwork.lib" "computenetwork.h" HAS_COMPUTENETWORK)
  check_pragma_lib("virtdisk.lib" "virtdisk.h" HAS_VIRTDISK)

  if(NOT (HAS_COMPUTECORE AND HAS_COMPUTENETWORK AND HAS_VIRTDISK))
    message(FATAL_ERROR
      "[hcs] One or more required libraries are missing:\n"
      "  HAS_COMPUTECORE=${HAS_COMPUTECORE}\n"
      "  HAS_COMPUTENETWORK=${HAS_COMPUTENETWORK}\n"
      "  HAS_VIRTDISK=${HAS_VIRTDISK}\n"
    )
  endif()

  add_library(hcs_backend STATIC
    api/hcn/hcn_wrapper.cpp
    api/hcn/hcn_api.cpp
    api/hcn/hcn_endpoint_info.cpp
    api/hcn/hcn_route.cpp
    api/hcn/hcn_subnet.cpp
    api/hcn/hcn_ipam.cpp
    api/hcn/hcn_network_policy.cpp
    api/hcn/hcn_create_endpoint_params.cpp
    api/hcn/hcn_create_network_params.cpp
    api/hcn/hcn_endpoint_query.cpp
    api/hcn/hcn_network_info.cpp
    api/hcs/hcs_wrapper.cpp
    api/hcs/hcs_event_type.cpp
    api/hcs/hcs_scsi_device.cpp
    api/hcs/hcs_schema_version.cpp
    api/hcs/hcs_network_adapter.cpp
    api/hcs/hcs_create_compute_system_params.cpp
    api/hcs/hcs_request.cpp
    api/hcs/hcs_api.cpp
    api/virtdisk/virtdisk_wrapper.cpp
    api/virtdisk/virtdisk_snapshot.cpp
    api/virtdisk/virtdisk_api.cpp
    api/virtdisk/virtdisk_guid.cpp
    api/virtdisk/virtdisk_child_rebuild.cpp
    api/virtdisk/virtdisk_utils.cpp
    hcs_virtual_machine.cpp
    hcs_virtual_machine_factory.cpp
    hcs_virtual_machine_resources.cpp
    util/guid_utils.cpp
  )

  target_link_libraries(hcs_backend PRIVATE
    fmt::fmt-header-only
    ztd::out_ptr
    Boost::json
    platform
    utils
    scope_guard
    computecore.lib
    computenetwork.lib
    virtdisk.lib
  )
endif()
