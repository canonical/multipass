# Build instructions for Windows 10/11 Pro

## Environment Setup

### Chocolatey

Install chocolatey, a package manager, to download the rest of the dependencies: <https://chocolatey.org/>.

Press Windows Key+X and Run Windows PowerShell(Admin) or Terminal(Admin) then follow the chocolatey
instructions to "Install with Powershell.exe".

### Dependencies

After chocolatey is installed you can now install the rest of the dependencies from the
Powershell(Admin). To get the best results, in the following order:

```[pwsh]
choco install cmake ninja qemu openssl git wget unzip -yfd
```

```[pwsh]
choco install visualstudio2022buildtools visualstudio2022-workload-vctools -yfd
```

NOTE: visualcpp-build-tools is only the installer package. For this reason, choco cannot detect any
new compiler tool updates so choco upgrade will report no new updates available. To update the
compiler and related tooling or fix a broken `visualstudio2022buildtools` installation do the following:

1. Go to "Add or remove programs"
2. Search for the Microsoft Visual Studio Installer
3. Click Modify
4. Click Modify in the Installer interface
5. For Windows 11, add "C++/CLI support for v143 build tools" in the "Desktop development with C++" kit
6. Complete the installation

### Git

You need to enable symlinks in Windows Git, have a look at
[the git-for-windows docs](https://github.com/git-for-windows/git/wiki/Symbolic-Links).

For Windows 11:

1. Go to "Developer Settings"
2. Enable "Developer mode"

### Qt6

To install Qt6, use `aqt`. First install it with chocolatey:

```[pwsh]
choco install aqt -yfd
```

Then specify the following options in the installation command:

```[pwsh]
aqt install-qt windows desktop 6.10.0 win64_msvc2022_64 -O C:/Qt
```

Alternatively, download the [qtbase archive](https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6100/qt6_6100/qt.qt6.6100.win64_msvc2022_64/6.10.0-0-202510021201qtbase-Windows-Windows_11_24H2-MSVC2022-Windows-Windows_11_24H2-X86_64.7z)
and extract it to `C:\Qt\6.10.0`.

### Path setup

You'll have to manually add CMake and Qt to your account's PATH variable.

Search for "Edit environment variables for your account" then edit your Path variable. Add the following:

- `C:\Program Files\CMake\bin`
- `C:\Qt\6.10.0\bin`

### Console setup

#### Cmder

Cmder is a sane terminal emulator for windows, which includes git and SSH support among other things.

Install with chocolatey:

```[pwsh]
choco install cmder -yfd
```

The following will setup a task that you can use to build things with the VS2022 compiler toolchain.

1. Run cmder which should be installed by default on C:\tools\cmder\Cmder.exe
2. Click the dropdown arrow next to the green "+" sign in the lower right corner of cmder
3. Select "Setup tasks..."
4. Click the "+" button to add a new task
5. In the task parameters box add (basically copying from cmd:Cmder task):
    ``/icon "%CMDER_ROOT%\icons\cmder.ico"``
6. In the commands box, add:

    ```[]
    cmd /k ""%ConEmuDir%\..\init.bat" && "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64" -new_console:d:%USERPROFILE%
    ```

7. Give the task a name (first box), such as vs2022 and click Save Settings.
8. Now run cmder, click on the green "+" and click on the vs2022 task
This will open a new terminal tab and run the VS2022 setup. CMake can now find the VS compiler.
Go to the [Building](./BUILD.windows.md#building) section.

#### x64 Native Tools Command Prompt

x64 Native Tools Command Prompt is the native console from the MVSC installation. On startup it
updates its environment variables to include all tools installed via MSVC for 64-bit. If you are
using 32-bit, use the x86 Native Tools Command Prompt.

If you want to use the environment variables in a different console, look at:
[Use the Microsoft C++ toolset from the command line](https://learn.microsoft.com/nb-no/cpp/build/building-on-the-command-line?view=msvc-170).

#### Powershell only

Using a Visual-Studio command-prompt-enabled PowerShell to build the project is also possible.
Open a new PowerShell terminal, then invoke the following commands:

```[pwsh]
# Try to determine the Visual Studio install path
$VSPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -products Microsoft.VisualStudio.Product.BuildTools -latest -property installationPath

# Import the PowerShell module
Import-Module "$VSPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"

# Enable VS developer PowerShell
Enter-VsDevShell -VsInstallPath "$VSPath" -DevCmdArguments '-arch=x64'
```

## Building

```[batch]
cd <multipass>
git submodule update --init --recursive
mkdir build
cd build
```

```[batch]
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_TOOLCHAIN_FILE=..\3rd-party\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_PREFIX_PATH=C:\Qt\6.2.4\msvc2019_64\ ../
```

```[batch]
cmake --build .
```

This builds `multipass` and `multipassd`.
To create an installer, run `cmake --build . --target package`.

## Running `multipass`

### Enable Hyper-V

Before starting `multipassd`, you'll have to enable the Hyper-V functionality in Windows 10/11 Pro.
See: [Install Hyper-V](https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/quick-start/enable-hyper-v)

Press Windows Key + X, Select Windows PowerShell (Admin) or Terminal(Admin) and run:

```[pwsh]
Enable-WindowsOptionalFeature -Online -FeatureName:Microsoft-Hyper-V -All
```

### Start the daemon (`multipassd`)

1. Press Windows Key + X, Select Windows PowerShell (Admin) or Terminal(Admin)
2. Run `multipassd` (for example: `multipassd --logger=stderr`)
3. Alternatively, you can install `multipassd` as a Windows Service (Run `multipassd /install` in a
   Powershell(Admin)). Installing `multipassd` as a Windows Service is a must for Windows 11
4. To stop and uninstall the Windows service, Run `multipassd /uninstall`

### Run `multipass`

With the `multipassd` daemon now running on another shell (or as a windows service) you can now run `multipass`.

1. Press Windows Key + X, Select Windows PowerShell, or Terminal.
2. Then, try `multipass help`.

### Permissions/privileges for `multipassd`

`multipassd` needs Administrator privileges in order to create symlinks when using mounts and to
manage Hyper-V instances.

If you don't need symlink support you can run `multipassd` on a less privileged shell but your user account
needs to be part of the Hyper-V Administrators group:

1. Press Windows key + X, Select "Computer Management"
2. Under System Tools->Local Users and Groups->Groups
3. Select on Hyper-V Administrators, add your account
4. Sign out or reboot for changes to take effect
