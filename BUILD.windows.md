Build instructions for Windows 10
=================================

Environment Setup
-----------------
### Chocolatey
Install chocolatey, a package manager, to download the rest of the dependencies: <https://chocolatey.org/>

Press Windows Key+X and Run Windows PowerShell(Admin) then follow the chocolatey instructions to "Install with Powershell.exe"

After chocolatey is installed you can now install the rest of the dependencies:

    choco install visualcpp-build-tools cmake ninja golang nasm cmder qemu-img -yfd

You may have to disable Windows Defender Real-time protection if you want the packages to install quicker.
Search for Windows Defender Security Center, go to Virus & threat protection, then Virus and thread protection settings, disable Real-time protection.

### Qt5
Install the latest stable version of Qt5.9 (5.9.1 at the moment): <http://www.qt.io/download-open-source/>.

In the installer, unselect everything. Install only the msvc2015 32 bit component and the msvc2017_64 component

### Path setup
You'll have to manually add CMake and Qt to your account's PATH variable.

Search for "Edit environment variables for your account" then edit the Path variable.
Add the following:
     C:\Program Files\CMake\bin
     C:\Qt\5.9.1\msvc2017_64\bin

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
    cmake -GNinja ../
    ninja


Running multipass
---------------------------------------

### Enable Hyper-V
Before starting multipassd, you'll have to enable the Hyper-V functionality in Windows 10 Pro.
See: https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/quick-start/enable-hyper-v

    Press Windows Key + X, Select Windows PowerShell (Admin)
    Run "Enable-WindowsOptionalFeature -Online -FeatureName:Microsoft-Hyper-V -All"

### Permissions/privileges
multipassd needs Administrator Privileges the first time it creates a VM instance to be able to
create a virtual network switch, assign it an IP address and create a NAT object.

To avoid the need to run multipassd with Administrator Privileges after creating the network switch,
your user account will need to be part of the Hyper-V Administrators group:

    Press Windows key + X, Select "Computer Management"
    Under System Tools->Local Users and Groups->Groups
    Select on Hyper-V Administrators, add your account
    Sign out or reboot for changes to take effect
