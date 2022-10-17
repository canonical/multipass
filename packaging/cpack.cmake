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

# Helpful docs:
# https://cmake.org/Wiki/CMake:Component_Install_With_CPack
# https://cmake.org/Wiki/CMake:CPackConfiguration
#
# Vital concept to understand are "Components" - a component is a collection of files to be installed.
# Any file to be packaged for installation must be part of a component - note the COMPONENT flag for
# "install". CPack then includes it in the generated package, to be installed at the destination
# specified in the "install" (plus a CPack-only prefix set below).
#
# Components are intended to be self-contained, as they may be installed individually. Components also
# smap directly to the options made visible to the user in the installer.
#

set(CPACK_WARN_ON_ABSOLUTE_INSTALL_DESTINATION ON) # helps avoid errors

set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENTS_ALL multipassd multipass multipass_gui)

set(CPACK_COMPONENT_MULTIPASSD_DISPLAY_NAME "Multipass Daemon")
set(CPACK_COMPONENT_MULTIPASSD_DESCRIPTION
   "Background process that creates and manages virtual machines")
set(CPACK_COMPONENT_MULTIPASS_DISPLAY_NAME "Clients (CLI and GUI)")
set(CPACK_COMPONENT_MULTIPASS_DESCRIPTION
   "Command line tool to talk to the multipass daemon")
set(CPACK_COMPONENT_MULTIPASS_GUI_DISPLAY_NAME "Multipass Status Menu")
set(CPACK_COMPONENT_MULTIPASS_GUI_DESCRIPTION
    "Status Menu integration for Multipass")

set(CPACK_COMPONENT_MULTIPASSD_REQUIRED TRUE)
set(CPACK_COMPONENT_MULTIPASS_REQUIRED TRUE)
set(CPACK_COMPONENT_MULTIPASS_GUI_REQUIRED TRUE)

# set default CPack Packaging options
set(CPACK_PACKAGE_NAME              "multipass")
set(CPACK_PACKAGE_VENDOR            "canonical")
set(CPACK_PACKAGE_CONTACT           "contact@canonical.com")
set(CPACK_PACKAGE_VERSION           "${MULTIPASS_VERSION}")

if (APPLE)
  set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}.${HOST_ARCH}")
endif()

#set(CPACK_PACKAGE_ICON              "${PROJECT_SOURCE_DIR}/cmake/sac_logo.png")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CPACK_STRIP_FILES 1)
endif ()

# set (CPACK_PACKAGE_DESCRIPTION_FILE ...)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Easily create, control and connect to Ubuntu instances")

if (MSVC)
  # multipassd depends on qemu-img.exe indirectly (called to convert qcow images to VHDX format)
  find_program(QEMU_IMG qemu-img.exe PATH_SUFFIXES "qemu")
  if (NOT QEMU_IMG)
    message(FATAL_ERROR "qemu-img not found!")
  endif()

  # make CMake prefer using the custom version of NSIS.template.in
  set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/packaging/windows")

  # InstallRequiredSystemLibraries finds the VC redistributable dlls shipped with the Visual Studio compiler tools
  # and creats an install(PROGRAMS ...) rule using the destination and component IDs setup below.
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT multipassd)
  if(cmake_build_type_lower STREQUAL "debug")
    set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  endif()
  include(InstallRequiredSystemLibraries)

  # The qemu-img path found may actually be a chocolatey generated shim.  Thankfully such shims have a command line
  # option that returns the actual executable target of the shim.
  execute_process(COMMAND ${QEMU_IMG} --shimgen-noop
    OUTPUT_VARIABLE QEMU_IMG_OUTPUT)
  string(REGEX MATCH "path to executable: (.*)[\n\r\t ]+working" QEMU_IMG_OUTPUT_REGEX_MATCH "${QEMU_IMG_OUTPUT}")
  if (CMAKE_MATCH_1)
    set(REAL_QEMU_IMG ${CMAKE_MATCH_1})
    string(REPLACE "\\" "/" REAL_QEMU_IMG ${REAL_QEMU_IMG})
    string(STRIP ${REAL_QEMU_IMG} REAL_QEMU_IMG)
  else()
    set(REAL_QEMU_IMG ${QEMU_IMG})
  endif()

  # Finally copy the real qemu-img executable into our package
  install(FILES "${REAL_QEMU_IMG}" DESTINATION bin COMPONENT multipassd)

  # fixup_bundle determines the dependencies of the given executable and copies them into the final package.
  # fixup_bundle assumes the copy destination is the same directory of the given executable, hence why it's pre-fixed
  # with CMAKE_INSTALL_PREFIX as that contains the correct directory where the package is assembled.
  # The third parameter of fixup_bundle is a directory to search for dependencies; here we assume qemu-img.exe deps
  # are contained in the same source directory where it was copied from
  get_filename_component(QEMU_IMG_DIR ${REAL_QEMU_IMG} DIRECTORY)
  install(CODE "
    include(BundleUtilities)
    fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/bin/qemu-img.exe\"  \"\"  \"${QEMU_IMG_DIR}\")
    " COMPONENT multipassd)

  # copy the icon and font, to use in windows terminal profiles
  install(FILES "${CMAKE_SOURCE_DIR}/packaging/windows/icon_wt.ico" DESTINATION bin RENAME multipass_wt.ico COMPONENT multipass)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/packaging/windows/fonts/" DESTINATION fonts COMPONENT multipass)

  set(CPACK_PACKAGE_ICON "${PROJECT_SOURCE_DIR}\\\\packaging\\\\windows\\\\multipass.bmp")
  set(CPACK_RESOURCE_FILE_WELCOME "${PROJECT_SOURCE_DIR}/packaging/windows/WELCOME.txt")
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/packaging/windows/LICENCE.rtf")
  set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/packaging/windows/README.txt")
  set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/packaging/windows/icon.ico")

  # Inserts an extra page in the installer asking the user if they want to modify their users or system PATH variable
  # This is useful to make "multipass.exe" findable on a shell
  set(CPACK_NSIS_MODIFY_PATH ON)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/CanonicalLtd/multipass")
  set(CPACK_NSIS_HELP_LINK "https://github.com/CanonicalLtd/multipass")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "Multipass")

  # The EventLog registry entries register a Multipass EventSource, which prevents the Event Viewer app complaining
  # about missing EVENT ID sources, which makes it harder to read the log entries.
  # The App Paths registry entries are to register multipass.exe as a valid command that can be called via ShellExecute
  # The Run key is to autostart the gui - explicit WOW6432Node as it'd go there anyway - see https://is.gd/lMTxdb
  # create shortcut installs a start-menu item for the multipass gui
  SET(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
    "
    WriteRegStr HKLM 'SYSTEM\\\\CurrentControlSet\\\\Services\\\\EventLog\\\\Application\\\\Multipass' 'EventMessageFile' '%SystemRoot%\\\\\\\\System32\\\\EventCreate.exe'
    WriteRegDWORD HKLM 'SYSTEM\\\\CurrentControlSet\\\\Services\\\\EventLog\\\\Application\\\\Multipass' 'TypesSupported' '7'
    WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\multipass.exe' '' '$INSTDIR\\\\bin\\\\multipass.exe'
    WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\multipass.exe' 'Path' '$INSTDIR\\\\bin'
    WriteRegStr HKLM 'SOFTWARE\\\\WOW6432Node\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Run' 'multipass-gui' '$INSTDIR\\\\bin\\\\multipass.gui.exe --autostarting'
    nsExec::ExecToLog '\\\"$INSTDIR\\\\bin\\\\multipassd.exe\\\" /install'
    Pop '$0'
    DetailPrint '\\\"Daemon install result: $0\\\"'
    CopyFiles '$INSTDIR\\\\Fonts\\\\*' '$WINDIR\\\\Fonts'
    WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows NT\\\\CurrentVersion\\\\Fonts' 'Ubuntu Mono (TrueType)' 'UbuntuMono-R.ttf'
    CreateShortCut '$SMPROGRAMS\\\\Multipass.lnk' '$INSTDIR\\\\bin\\\\multipass.gui.exe'
    WriteRegStr HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Multipass' 'DisplayIcon' '$INSTDIR\\\\bin\\\\multipass_wt.ico'
    "
  )

  # TODO: There must be a better way than hardcoding the cache and data directory here
  # NSIS creates 32-bit installer/uninstaller applications. Windows, automatically redirects file system calls into
  # system32 to syswow64 (the 32-bit system for windows 64 folder) for 32-bit apps unless you explicitly disable such redirection
  # Since multipassd is a 64-bit application, it uses the real system32 folder, which is where the settings and cache
  # directory live
  # The directories are removed after uninstalling the service to prevent removing data while multipassd is alive.
  # If the daemon is still running after uninstallation, it's stuck and we need to kill it, otherwise it will break upgrades
  # The clients are killed, but they're not holding any state or data, so should not be a problem.
  SET(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
    "
    Var /GLOBAL REMOVE_SETTINGS_AND_CACHE
    StrCpy $REMOVE_SETTINGS_AND_CACHE 0
    MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 \\\"Do you want to remove all multipass VM instances, settings and cached data?\\\" \\
    /SD IDNO IDNO basic_uninst
    nsExec::ExecToLog  '\\\"$INSTDIR\\\\bin\\\\multipass.exe\\\" delete -p --all'
    StrCpy $REMOVE_SETTINGS_AND_CACHE 1

    basic_uninst:
    Delete '$SMPROGRAMS\\\\Multipass.lnk'
    nsExec::ExecToLog  '\\\"$INSTDIR\\\\bin\\\\multipassd.exe\\\" /uninstall'
    nsExec::ExecToLog 'TaskKill /IM multipassd.exe /F'
    nsExec::ExecToLog 'TaskKill /IM multipass.exe /F'
    nsExec::ExecToLog 'TaskKill /IM multipass.gui.exe /F'
    DeleteRegKey HKLM 'SYSTEM\\\\CurrentControlSet\\\\Services\\\\EventLog\\\\Application\\\\Multipass'
    DeleteRegKey HKLM 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\multipass.exe'
    DeleteRegValue HKLM 'SOFTWARE\\\\WOW6432Node\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Run' 'multipass-gui'
    StrCmp $REMOVE_SETTINGS_AND_CACHE \\\"0\\\" done_uninst
    !include \\\"x64.nsh\\\"
    \\\${DisableX64FSRedirection}
    SetShellVarContext all
    RMDir /r \\\"$SYSDIR\\\\config\\\\systemprofile\\\\AppData\\\\Local\\\\multipassd\\\"
    RMDir /r \\\"$SYSDIR\\\\config\\\\systemprofile\\\\AppData\\\\Roaming\\\\multipassd\\\"
    RMDIR /r \\\"$LOCALAPPDATA\\\\Multipass\\\"
    \\\${EnableX64FSRedirection}
    done_uninst:
    "
  )
endif()

if(APPLE)
  set(CPACK_OSX_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(CPACK_RESOURCE_FILE_WELCOME "${PROJECT_SOURCE_DIR}/packaging/macos/WELCOME.html")
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/packaging/macos/LICENCE.html")
  set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/packaging/macos/README.html")
  set(CPACK_GENERATOR "productbuild")
  set(CPACK_productbuild_COMPONENT_INSTALL ON)

  set(CPACK_PACKAGING_INSTALL_PREFIX   "/Library/Application Support/com.canonical.multipass")
  list(APPEND CPACK_INSTALL_COMMANDS "bash -x ${CMAKE_SOURCE_DIR}/packaging/macos/fixup-qt6-libs-rpath.sh ${CMAKE_BINARY_DIR}")
  list(APPEND CPACK_INSTALL_COMMANDS "bash -x ${CMAKE_SOURCE_DIR}/packaging/macos/install-ssl-libs.sh ${CMAKE_BINARY_DIR}")
  list(APPEND CPACK_INSTALL_COMMANDS "bash -x ${CMAKE_SOURCE_DIR}/packaging/macos/fixup-qemu-and-deps.sh ${CMAKE_BINARY_DIR}")

  set(MULTIPASSD_PLIST "com.canonical.multipassd.plist")
  set(MULTIPASSGUI_PLIST "com.canonical.multipass.gui.autostart.plist")
  configure_file("${CMAKE_SOURCE_DIR}/packaging/macos/${MULTIPASSD_PLIST}.in"
                 "${CMAKE_BINARY_DIR}/${MULTIPASSD_PLIST}" @ONLY)
  configure_file("${CMAKE_SOURCE_DIR}/packaging/macos/postinstall-multipassd.sh.in"
                 "${CMAKE_BINARY_DIR}/postinstall-multipassd.sh" @ONLY)
  configure_file("${CMAKE_SOURCE_DIR}/packaging/macos/postinstall-multipass.sh.in"
                 "${CMAKE_BINARY_DIR}/postinstall-multipass.sh" @ONLY)
  configure_file("${CMAKE_SOURCE_DIR}/packaging/macos/postinstall-multipass-gui.sh.in"
                 "${CMAKE_BINARY_DIR}/postinstall-multipass-gui.sh" @ONLY)

  install(FILES "${CMAKE_BINARY_DIR}/${MULTIPASSD_PLIST}" DESTINATION Resources COMPONENT multipassd)
  install(FILES "${CMAKE_SOURCE_DIR}/packaging/macos/${MULTIPASSGUI_PLIST}" DESTINATION Resources COMPONENT multipass_gui)
  install(FILES "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist" DESTINATION Resources COMPONENT multipass_gui)
  install(FILES "${CMAKE_SOURCE_DIR}/packaging/macos/icon.icns" DESTINATION Resources COMPONENT multipass_gui)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/completions" DESTINATION Resources COMPONENT multipass)
  install(DIRECTORY "${CMAKE_BINARY_DIR}/lib/" DESTINATION lib COMPONENT multipassd)

  set(CPACK_PREFLIGHT_MULTIPASSD_SCRIPT  "${CMAKE_SOURCE_DIR}/packaging/macos/preinstall-multipassd.sh")
  set(CPACK_POSTFLIGHT_MULTIPASSD_SCRIPT "${CMAKE_BINARY_DIR}/postinstall-multipassd.sh")
  set(CPACK_POSTFLIGHT_MULTIPASS_SCRIPT  "${CMAKE_BINARY_DIR}/postinstall-multipass.sh")
  set(CPACK_POSTFLIGHT_MULTIPASS_GUI_SCRIPT  "${CMAKE_BINARY_DIR}/postinstall-multipass-gui.sh")

  # Cleans up the installed package
  set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_SOURCE_DIR}/packaging/cleanup.cmake")

  # Signs the binaries using ad-hoc signing
  set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_SOURCE_DIR}/packaging/macos/post_build.cmake")

  # CPack doesn't support a direct way to customise the Distribution.dist file, but the template
  # CPack.distribution.dist.in is searched for in the CMAKE_MODULE_PATH before CMAKE_ROOT, so as a hack,
  # point it to a local directory with our custom template
  set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/packaging/macos")

  install(FILES "${CMAKE_SOURCE_DIR}/packaging/macos/uninstall.sh"
          DESTINATION . COMPONENT multipassd)
endif()

# must be last
include(CPack)
