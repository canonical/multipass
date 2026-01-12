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
choco install cmake ninja qemu-img git wget unzip -yfd
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
6. Make sure that `vcpkg` is not installed in the same selection
7. Complete the installation

### Git

You need to enable symlinks in Windows Git, have a look at
[the git-for-windows docs](https://github.com/git-for-windows/git/wiki/Symbolic-Links).

For Windows 11:

1. Go to "Developer Settings"
2. Enable "Developer mode"

### Path setup

You'll have to manually add CMake to your account's PATH variable.

Search for "Edit environment variables for your account" then edit your Path variable. Add the following:

- `C:\Program Files\CMake\bin`

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
cmake -GNinja ..
```

CMake will automatically fetch all necessary content, build vcpkg dependencies, and initialize
the build system. If it doesn't pick it up automatically,
specify the vcpkg toolchain manually with
`-DCMAKE_TOOLCHAIN_FILE=..\3rd-party\vcpkg\scripts\buildsystems\vcpkg.cmake `.
To specify the build type, use `-DCMAKE_BUILD_TYPE` option to set the build type (e.g.,
`Debug`, `Release`, etc.).

To use a different vcpkg, pass `-DMULTIPASS_VCPKG_LOCATION="path/to/vcpkg"` to CMake.
It should point to the root vcpkg location, where the top bootstrap scripts are located.

Another example:

```[batch]
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_TOOLCHAIN_FILE=..\3rd-party\vcpkg\scripts\buildsystems\vcpkg.cmake ../
```

Finally, to build the project, run:

```[batch]
cmake --build . --parallel
```

This builds `multipass`, `multipassd`, and `multipass_cpp_tests`.
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
2. [Extend the Path variable](./BUILD.windows.md#Extend the Path variable).
3. Then, try `multipass help`.

### Permissions/privileges for `multipassd`

`multipassd` needs Administrator privileges in order to create symlinks when using mounts and to
manage Hyper-V instances.

If you don't need symlink support you can run `multipassd` on a less privileged shell but your user account
needs to be part of the Hyper-V Administrators group:

1. Press Windows key + X, Select "Computer Management"
2. Under System Tools->Local Users and Groups->Groups
3. Select on Hyper-V Administrators, add your account
4. Sign out or reboot for changes to take effect

### Run `multipass.gui`

1. [Extend the Path variable](./BUILD.windows.md#Extend the Path variable).
2. Now you can run `multipass.gui`.

### Extend the Path variable

To avoid conflict with a multipass installation, include the necessary paths to the Path variable in your current session only:
```
$env:Path += ";<multipass>\build\bin"
$env:Path += ";<multipass>\build\bin\windows\x64\runner\Release"
```
