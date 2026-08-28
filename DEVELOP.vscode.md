# Development setup for VSCode

This document describes how to setup VSCode if you wish to contribute to Multipass using this IDE.

## Requirements

### Hardware

- At least 16GB of RAM.
- 8GB of swap space.

### Software

- Install [Visual Studio Code](https://code.visualstudio.com/download) for your OS.
- Open `multipass` folder in VSCode.
- To install the extensions recommended by the workspace:
    - Open the command prompt (`Ctrl+Shift+P` by default).
    - Execute `Extensions: Show Recommended Extensions`.
    - Install the extensions listed under `Workspace Recommendations`.
    - Reload VSCode.

## Building

Once `CMake Tools` is installed, a CMake tab should be available from the toolbar.
In it, you can select under `Configure` the following preset: `local-multi-config`.
This preset uses a multi-configuration generator, which will allow to switch from `Release` to `Debug` easily.

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
sudo ./build/bin/multipassd

# On Windows
TODO
```

> [!INFO]
> At most one single instance of the multipass daemon can be launched at any time.
> So before running this, make sure you stopped other instances, including the officially installed daemon. Otherwise, the service will fail to start.

A [launch.json](./.vscode/launch.json) providing several configurations is available.
Once the daemon is started, you can choose a configuration to launch with the command `Debug: Select and Start Debugging`. You can also launch the currently selected configuration with `F5`. 
The choice are:
- `Debug CLI`: launch and attach to the CLI. VSCode will prompt you for the arguments of the program.
- `Debug GUI`: launch and attach to the GUI.
- `Attach to daemon`: attach to the previously started daemon. You will be prompted for authentication.

## Tests execution

For now, 4 tests suites are available in the project.  
If you properly installed the `ms-vscode.cmake-tools` extension, then they should appear in the `Testing` tab of VSCode. From there, you can either launch or debug them.
