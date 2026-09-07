This guide helps new contributors quickly set up a Multipass development environment.  
It currently covers command-line debugging with GDB and development with Visual Studio Code. Contributions covering other IDEs are welcome.

# Common requirements

- At least 16GB of RAM.
- 8GB of swap space.
- Requirements listed under `BUILD.<os>.md`.

# CMake and GDB

## Specific requirements

- Install [GDB](https://sourceware.org/gdb/download/) for your operating system.

## Building

You can build using the `local-debug` or `local-release` preset.
```sh
# Debug build
cmake --preset local-debug
cmake --build build/debug [--parallel <N>]

# Release build
cmake --preset local-release
cmake --build build/release [--parallel <N>]
```

> [!INFO]
> `--parallel` can be used to parallelize the build. But keep in mind that building this project is memory-intensive. You may run out-of-memory if you set a value too high there.

## Debugging

TODO

# VSCode

## Specific requirements

- Install [Visual Studio Code](https://code.visualstudio.com/download) for your OS.
- Open `multipass` folder in VSCode.
- To install the extensions recommended by the workspace:
    - Open the command prompt (`Ctrl+Shift+P` by default).
    - Execute `Extensions: Show Recommended Extensions`.
    - Install the extensions listed under `Workspace Recommendations`.
    - Reload VSCode.

## Building

Once `CMake Tools` is installed, a CMake tab should be available from the primary side bar.
In it, you can select under `Configure` one of the following preset: `local-debug` or `local-release`, depending on the build type you want to generate.

To parallelize the build accross several jobs, you may configure the `cmake.parallelJobs` settings:
- Open the user settings (`Ctrl+Shift+P` to open the command prompt, then `Preferences: Open User Settings`).
- Search for `cmake.parallelJobs`, then specify the value of your choice.

> [!WARNING]
> Keep in mind that building this project is memory-intensive. You may run out-of-memory if you set a value too high there.

## Debugging

The `multipass` project generates three main executables:
- the daemon `multipassd`, that needs to be executed with root priviledges,
- the CLI `multipass`,
- the GUI `multipass_gui`.

For the CLI and the GUI to be used, the daemon must be started first.
To do so, open a separate shell and run the following into it:
```sh
# On Linux / MacOS
sudo ./build/<config>/bin/multipassd

# On Windows
Start-Process ./build/<config>/bin/multipassd -Verb RunAs
```

> [!INFO]
> At most one single instance of the multipass daemon can be launched at any time.
> So before running this, make sure you stopped other instances, including the officially installed daemon. Otherwise, the service will fail to start.

**For Windows**  
- Remember to register the service first (run `multipassd /install` as administrator), or you will encounter authentication issues when using the client.

A [launch.json](./.vscode/launch.json) providing several configurations is available.
Once the daemon is started, you can choose a configuration to launch with the command `Debug: Select and Start Debugging`. You can also launch the currently selected configuration with `F5`. 
The choice are:
- `Debug CLI`: launch and attach to the CLI. VSCode will prompt you for the arguments of the program.
- `Debug GUI`: launch and attach to the GUI.
- `Attach to daemon`: attach to the previously started daemon. You will be prompted for authentication.

### Troubleshoot

**`command` failed: The user is not authenticated with the Multipass service.**  
On Windows, remember to register the service first (run `multipassd /install` as administrator), or you will encounter this error when using the client.

**I can't attach to the daemon on Windows.**  
At the moment, there is no known way to attach to an elevated process from a non-elevated debugger (cf [open issue](https://github.com/microsoft/vscode-cpptools/issues/2881)).  
Note that running VS Code as an administrator grants elevated privileges to its extensions, which may introduce security risks.

## Tests execution

For now, 4 tests suites are available in the project.  
If you properly installed the `ms-vscode.cmake-tools` extension, then they should appear in the `Testing` tab of VSCode. From there, you can either launch or debug them.
