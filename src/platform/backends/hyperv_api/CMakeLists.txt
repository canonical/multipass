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
      "[hyperv_api] One or more required libraries are missing:\n"
      "  HAS_COMPUTECORE_LIB=${HAS_COMPUTECORE_LIB}\n"
      "  HAS_COMPUTENETWORK_LIB=${HAS_COMPUTENETWORK_LIB}\n"
      "  HAS_VIRTDISK_LIB=${HAS_VIRTDISK_LIB}\n"
    )
  endif()

  add_library(hyperv_api_backend STATIC
    hcn/hyperv_hcn_api_wrapper.cpp
    hcn/hyperv_hcn_route.cpp
    hcn/hyperv_hcn_subnet.cpp
    hcn/hyperv_hcn_ipam.cpp
    hcn/hyperv_hcn_network_policy.cpp
    hcs/hyperv_hcs_api_wrapper.cpp
    hcs/hyperv_hcs_scsi_device.cpp
    hcs/hyperv_hcs_network_adapter.cpp
    hcs/hyperv_hcs_plan9_share_params.cpp
    hcs/hyperv_hcs_create_compute_system_params.cpp
    virtdisk/virtdisk_api_wrapper.cpp
    virtdisk/virtdisk_snapshot.cpp
    hcs_plan9_mount_handler.cpp
    hcs_virtual_machine.cpp
    hcs_virtual_machine_factory.cpp
  )

  target_link_libraries(hyperv_api_backend PRIVATE
    fmt::fmt-header-only
    utils
    computecore.lib
    computenetwork.lib
    virtdisk.lib
  )
endif()
