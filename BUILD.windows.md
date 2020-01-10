Build instructions for Windows 10
=================================

Environment Setup
-----------------
### Chocolatey
Install chocolatey, a package manager, to download the rest of the dependencies: <https://chocolatey.org/>

Press Windows Key+X and Run Windows PowerShell(Admin) then follow the chocolatey instructions to "Install with Powershell.exe"

After chocolatey is installed you can now install the rest of the dependencies:

    choco install visualcpp-build-tools cmake ninja golang cmder qemu-img nsis -yfd

You may have to disable Windows Defender Real-time protection if you want the packages to install quicker.
Search for Windows Defender Security Center, go to Virus & threat protection, then Virus and thread protection settings, disable Real-time protection.

NOTE: visualcpp-build-tools is only the installer package. For this reason, choco cannot detect any new compiler tool updates so choco upgrade
will report no new updates available. To update the compiler and related tooling, you will need to search for "Add or remove programs",
find "Microsoft Visual Studio Installer" and click "Modify".

### Git
You need to enable symlinks in Windows Git, have a look at [the git-for-windows docs](https://github.com/git-for-windows/git/wiki/Symbolic-Links).

### Assembler
Previously, nasm had been used to build crypto modules in the boringssl module of grpc. However, there is a CMake bug that passes compiler options to nasm and nasm is unable to handle these options and fails.

yasm is another assembler that works and will only output a warning on these options and continue building. We will be using yasm from now on, so nasm will need to be removed and yasm will need to be installed.

    choco uninstall nasm -y
    C:\Program Files\NASM\Uninstall.exe

The Chocolatey version of yasm is quite old and no longer able to assemble the latest updated gRPC code. If the Chocolatey version of yasm is installed, you will need to uninstall it:

    choco uninstall yasm -y

You will need to download the latest version of yasm from: https://www.tortall.net/projects/yasm/releases/yasm-1.3.0-win64.exe

This is a stand-alone binary, so you'll need to either copy it to a directory in your PATH or use "-DCMAKE_ASM_NASM_COMPILER=/path/to/yasm" in the cmake command. However, if you do copy it in your path, you will need to rename it to 'yasm.exe' in order for cmake to detect it. Alternatively, create a symlink with `mklink` in an administrator command prompt (e.g. `mklink yasm.exe yasm-1.3.0-win64.exe`)

You may need to clean your build directory and run cmake again to pick up the yasm assembler path.

### Qt5
Install the latest stable version of Qt5 (5.12.4 at the moment): <https://www.qt.io/download-thank-you?os=windows/>.

In the online installer, under Qt, select MSVC 2017 64-bit.

If you already have Qt installed, run the MaintenanceTool included in the Qt directory to update to the latest version.

Alternatively, download the [qtbase archive](https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt5_5124/qt.qt5.5124.win64_msvc2017_64/5.12.4-0-201906140149qtbase-Windows-Windows_10-MSVC2017-Windows-Windows_10-X86_64.7z) and extract it to `C:\Qt` (so it ends up in `C:\Qt\5.12.4`).

### OpenSSL
Qt needs OpenSSL for doing https connections.

Download pre-built binaries from: <http://wiki.overbyte.eu/arch/openssl-1.1.1c-win64.zip>

Unzip that file to the top of the Multipass source directory, to a directory with the same name but for the extension, (e.g. `7z x -o"<multipass_src>\openssl-1.1.1c-win64" openssl-1.1.1c-win64.zip`)

The dlls within are redistributed with the package. but if you want a locally built multipassd to use https, you will need to copy them manually to the bin directory (or somewhere on the `Path`).

### Path setup
You'll have to manually add CMake and Qt to your account's PATH variable.

Search for "Edit environment variables for your account" then edit your Path variable.
Add the following:
     `C:\Program Files\CMake\bin`
     `C:\Qt\5.12.4\msvc2017_64\bin`

### Cmder setup
Cmder is a sane terminal emulator for windows, which includes git and SSH support among other things.

The following will setup a task that you can use to build things with the VS2017 compiler toolchain.

Run cmder which should be installed by default on C:\tools\cmder\Cmder.exe
Click on the green "+" sign on the right lower corner of cmder
Select "Setup tasks..."
Click the "+" button to add a new task
In the  task parameters box add (basically copying from cmd:Cmder task):
    /icon "%CMDER_ROOT%\icons\cmder.ico"
In the commands box, add:
    cmd /k ""%ConEmuDir%\..\init.bat" && "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64" -new_console:d:%USERPROFILE%

Give the task a name (first box), such as vs2017 and click Save Settings.

Building
---------------------------------------

Run cmder, click on the green "+" and click on the vs2017 task
This will open a new terminal tab and run the VS2017 setup. CMake can now find the VS compiler.

    cd <multipass>
    git submodule update --init --recursive
    mkdir build
    cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo ../
    ninja

This builds multipass and multipassd.
To create an installer, run `ninja package`

Running multipass
---------------------------------------

### Enable Hyper-V
Before starting multipassd, you'll have to enable the Hyper-V functionality in Windows 10 Pro.
See: https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/quick-start/enable-hyper-v

    Press Windows Key + X, Select Windows PowerShell (Admin)
    Run "Enable-WindowsOptionalFeature -Online -FeatureName:Microsoft-Hyper-V -All"

### Start the daemon (multipassd)
    Press Windows Key + X, Select Windows PowerShell (Admin)
    Run multipassd (for example: multipassd --logger=stderr)
    Alternatively, you can install multipassd as a Windows Service (Run "multipassd /install")
    To stop and uninstall the Windows service, Run "multipassd /uninstall"

### Run multipass
    With the multipassd daemon now running on another shell (or as a windows service) you can now run multipass.
    Press Windows Key + X, Select Windows PowerShell, or alternatively run cmd.exe on the search bar
    Try "multipass help"

### Permissions/privileges for multipassd
multipassd needs Administrator privileges in order to create symlinks when using mounts and to manage Hyper-V instances.

If you don't need symlink support you can run multipassd on a less privileged shell but your user account
needs to be part of the Hyper-V Administrators group:

    Press Windows key + X, Select "Computer Management"
    Under System Tools->Local Users and Groups->Groups
    Select on Hyper-V Administrators, add your account
    Sign out or reboot for changes to take effect
