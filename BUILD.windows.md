Build instructions for Windows 10
=================================

Environment Setup
-----------------

### Chocolatey

Install chocolatey, a package manager, to download the rest of the dependencies: <https://chocolatey.org/>

Press Windows Key+X and Run Windows PowerShell(Admin) then follow the chocolatey instructions to "Install with
Powershell.exe"

### Dependencies
After chocolatey is installed you can now install the rest of the dependencies from the Powershell(Admin). To get the best results, in the following order:
```
choco install cmake ninja qemu openssl git -yfd
```
```
choco install visualstudio2019buildtools visualstudio2019-workload-vctools -yfd
```

You may have to disable Windows Defender Real-time protection if you want the packages to install quicker. Search for
Windows Defender Security Center, go to Virus & threat protection, then Virus and thread protection settings, disable
Real-time protection.

NOTE: visualcpp-build-tools is only the installer package. For this reason, choco cannot detect any new compiler tool
updates so choco upgrade will report no new updates available. To update the compiler and related tooling or fix a broken `visualstudio2019buildtools` installation do the following:
1. Go to "Add or remove programs"
2. Search for the Microsoft Visual Studio Installer
3. Click Modify
4. Click Modify in the Installer interface
5. (W11) Add "C++/CLI support for v142 build tools" in the "Desktop development with C++" kit
6. Complete the installation


### Git

You need to enable symlinks in Windows Git, have a look at
[the git-for-windows docs](https://github.com/git-for-windows/git/wiki/Symbolic-Links).

(Windows 11)
1. Go to "Developer Settings"
2. Enable "Developer mode"

### Qt6

To install Qt6, use `aqt`. First install it with chocolatey:
```
choco install aqt -yfd
```
Then specify the following options in the installation command:
```
aqt install-qt windows desktop 6.2.4 win64_msvc2019_64 -O C:/Qt
```

### Path setup

You'll have to manually add CMake and Qt to your account's PATH variable.

Search for "Edit environment variables for your account" then edit your Path variable. Add the following:

- `C:\Program Files\CMake\bin`
- `C:\Qt\6.2.4\msvc2019_64\bin`

### Cmder setup

Cmder is a sane terminal emulator for windows, which includes git and SSH support among other things.

The following will setup a task that you can use to build things with the VS2019 compiler toolchain.

Run cmder which should be installed by default on C:\tools\cmder\Cmder.exe
Click on the green "+" sign on the right lower corner of cmder
Select "Setup tasks..."
Click the "+" button to add a new task
In the task parameters box add (basically copying from cmd:Cmder task):
``/icon "%CMDER_ROOT%\icons\cmder.ico"``
In the commands box, add:
``cmd /k ""%ConEmuDir%\..\init.bat" && "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64" -new_console:d:%USERPROFILE%``

Give the task a name (first box), such as vs2019 and click Save Settings.

Building
---------------------------------------

Run cmder, click on the green "+" and click on the vs2019 task
This will open a new terminal tab and run the VS2019 setup. CMake can now find the VS compiler.

    cd <multipass>
    git submodule update --init --recursive
    mkdir build
    cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo ../
    ninja

This builds multipass and multipassd.
To create an installer, run `ninja package`

Building (alternative, PowerShell only)
---------------------------------------

Using a Visual-Studio command-prompt-enabled PowerShell to build the project is also possible.
Open a new PowerShell terminal, then invoke the following commands:

```pwsh
# Try to determine the Visual Studio install path
$VSPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -products Microsoft.VisualStudio.Product.BuildTools -latest -property installationPath

# Import the PowerShell module
Import-Module "$VSPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"

# Enable VS developer PowerShell
Enter-VsDevShell -VsInstallPath "$VSPath" -DevCmdArguments '-arch=x64'
```

Then, follow the steps in the previous "Building" step to build the project.

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
