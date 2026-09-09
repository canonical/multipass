This guide helps new contributors quickly set up a Multipass development environment.  
It currently covers command-line debugging with GDB and development with Visual Studio Code. Contributions covering other IDEs are welcome.

# Before starting

## Common requirements

- At least 16GB of RAM.
- 8GB of swap space.
- Requirements listed under `BUILD.<os>.md`.

## General information

The Multipass project generates three main executables:
- the daemon `multipassd`, that needs to be executed with elevated priviledges,
- the CLI `multipass`,
- the GUI `multipass_gui`.

The vast majority of Multipass's logic is executed by the daemon. The clients are essentially interfaces that establish a connection to the daemon, relay user commands to it, and return the daemon's response to the user. To use the clients, the daemon must be started first.

Before attempting to debug the daemon, please keep the following in mind:
1. A second instance will fail to start if another one is already running on the machine. Be sure to stop any active `multipassd` processes before launching a new one.
2. The daemon must run with elevated privileges.
3. To debug an elevated process, the debugger must also run with elevated privileges.

The last point can be challenging when debugging from within an IDE. Several approaches are available:
- Launch the IDE in administrator mode as well.
    - Advantage: this works in all cases.
    - Drawback: this also grants administrative privileges to extensions, which may introduce security concerns.
- Attach the IDE debugger to a debugger instance that is launched as administrator outside the IDE.
    - Advantage: only the debugger runs with elevated permissions.
    - Drawback: not all IDEs support this workflow.
- Launch the IDE in administrator mode inside a container or virtual machine.
    - Advantage: this provides a more isolated environment while keeping the IDE integrated.
    - Drawback: it requires a machine with sufficient resources to run the container or VM.

This guide describes several of these options. You may apply them directly or explore alternative approaches.

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

```sh
# Debug the daemon (requires elevated privileges)
sudo gdb build/debug/bin/multipassd

# Debug the CLI
gdb build/debug/bin/multipass

# Debug the GUI
gdb build/debug/bin/multipass.gui
```

> [!INFO]
> `multipassd` will shutdown when using Ctrl+C, even if configuring GDB to
> not forward SIGINT to the debugged process.

# Visual Studio Code

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

### Locally

A [launch.json](./.vscode/launch.json) providing several configurations is available.
You can choose a configuration to launch with the command `Debug: Select and Start Debugging`. You can also launch the currently selected configuration with `F5`.  
The choice are:
- `Debug daemon`: launch and attach to the daemon.
- `Debug CLI`: launch and attach to the CLI. VSCode will prompt you for the arguments of the program.
- `Debug GUI`: launch and attach to the GUI.

**Troubleshoot**
- At most one single instance of the multipass daemon can be running at any time. So before launching one, make sure you stopped other instances, including the officially installed daemon.
- On Windows, you need to run `multipassd /install` as administrator before using one of the clients. Otherwise, an authentication error will be raised.
- On Windows, the `Debug daemon` configuration will not work in an IDE run without elevated privileges. We advise you to develop from an isolated environment (such as a VM) if you want to use VSCode as administator.

### Using Development Containers

- Install the following extension: `ms-vscode-remote.remote-containers`.
- 

## Tests execution

For now, 4 tests suites are available in the project.  
If you properly installed the `ms-vscode.cmake-tools` extension, then they should appear in the `Testing` tab of VSCode. From there, you can either launch or debug them.
