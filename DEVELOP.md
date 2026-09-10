# Multipass development guide

This guide helps new contributors set up a Multipass development environment. It currently covers command-line debugging with GDB and development with Visual Studio Code. Contributions for other IDEs are welcome.

## Contents

- [Before starting](#before-starting)
  - [Common requirements](#common-requirements)
  - [General information](#general-information)
- [CMake and GDB](#cmake-and-gdb)
  - [Specific requirements](#specific-requirements)
  - [Building](#building)
  - [Debugging](#debugging)
- [Visual Studio Code](#visual-studio-code)
  - [Specific requirements](#specific-requirements-1)
  - [Using development containers (optional)](#using-development-containers-optional)
  - [Building](#building-1)
  - [Debugging](#debugging-1)
  - [Running tests](#running-tests)

## Before starting

### Common requirements

- At least 16 GB of RAM.
- 8 GB of swap space.
- Requirements listed in `BUILD.<os>.md`.

### General information

The Multipass project generates three main executables:
- the daemon `multipassd`, which must be run with elevated privileges;
- the CLI `multipass`;
- the GUI `multipass_gui`.

The vast majority of Multipass's logic is executed by the daemon. The clients are interfaces that establish a connection to the daemon, relay user commands to it, and return the daemon's response to the user. The daemon must be started before you can use the clients.

Before debugging the daemon, keep the following points in mind:
1. A second instance will fail to start if another one is already running on the machine. Stop any active `multipassd` processes before launching a new one.
2. The daemon must run with elevated privileges.
3. To debug an elevated process, the debugger must also run with elevated privileges.

The last point can be challenging when debugging from an IDE. Several approaches are available:
- Launch the IDE in administrator mode.
  - Advantage: this works in all cases.
  - Drawback: this also grants administrative privileges to extensions, which may introduce security concerns.
- Attach the IDE debugger to a debugger instance launched as administrator outside the IDE.
  - Advantage: only the debugger runs with elevated permissions.
  - Drawback: not all IDEs support this workflow.
- Launch the IDE in administrator mode inside a container or virtual machine.
  - Advantage: this provides a more isolated environment while keeping the IDE integrated.
  - Drawback: this requires a machine with sufficient resources to run the container or VM.

This guide describes several of these options. You can apply them directly or explore alternative approaches.

## CMake and GDB

### Specific requirements

- Install [GDB](https://sourceware.org/gdb/download/) for your operating system.

### Building

You can build using the `local-debug` or `local-release` preset:
```sh
# Debug build
cmake --preset local-debug
cmake --build build/debug [--parallel <N>]

# Release build
cmake --preset local-release
cmake --build build/release [--parallel <N>]
```

> [!INFO]
> `--parallel` can be used to parallelize the build. Keep in mind that building this project is memory-intensive: a value that is too high may cause the system to run out of memory.

### Debugging

```sh
# Debug the daemon (requires elevated privileges)
sudo gdb build/debug/bin/multipassd

# Debug the CLI
gdb build/debug/bin/multipass

# Debug the GUI
gdb build/debug/bin/multipass.gui
```

> [!INFO]
> `multipassd` will shut down when you press Ctrl+C, even if GDB is configured not to forward `SIGINT` to the debugged process.

## Visual Studio Code

### Specific requirements

- Install [Visual Studio Code](https://code.visualstudio.com/download) for your OS.
- Open the `multipass` folder in Visual Studio Code.
- To install the extensions recommended by the workspace:
  - Open the Command Palette (`Ctrl+Shift+P` by default).
  - Run `Extensions: Show Recommended Extensions`.
  - Install the extensions listed under `Workspace Recommendations`.
  - Reload Visual Studio Code.

### Using development containers (optional)

Using a development container offers the following advantages:
- You don't need to install development or runtime dependencies on your local system.
- Your IDE instance runs in isolation from your local system, which means it can safely be run with elevated privileges.

However:
- Development containers are necessarily Linux images. Therefore, you cannot test a Windows build there.
- Your machine will need at least 32 GB of RAM and additional disk space.

To use development containers:
- Install a container engine, such as [Podman](https://podman.io/docs/installation) or [Docker](https://docs.docker.com/get-started/get-docker/).
- Install the `ms-vscode-remote.remote-containers` extension.
- If you are not using Docker, open User Settings and adjust `dev.containers.dockerPath` and `dev.containers.dockerComposePath`.

Once you have installed the requirements, reopen your workspace in the development container:
- Open the Command Palette (`Ctrl+Shift+P` by default).
- Run `Dev Containers: Reopen in Container`.

### Building

Once `CMake Tools` is installed, a CMake tab should be available in the primary sidebar. Under `Configure`, select the `local-debug` or `local-release` preset, depending on the build type you want to generate.

To parallelize the build across several jobs, configure the `cmake.parallelJobs` setting:
- Open User Settings (`Ctrl+Shift+P`, then `Preferences: Open User Settings`).
- Search for `cmake.parallelJobs`, then specify the value of your choice.

> [!WARNING]
> Keep in mind that building this project is memory-intensive: a value that is too high may cause the system to run out of memory.

### Debugging

A [launch.json](./.vscode/launch.json) file with several configurations is available.
Select a configuration with `Debug: Select and Start Debugging`, or launch the currently selected configuration with `F5`.

The available configurations are:
- `Debug daemon`: launch and attach to the daemon.
- `Debug CLI`: launch and attach to the CLI. VSCode will prompt you for the program arguments.
- `Debug GUI`: launch and attach to the GUI.

#### Troubleshooting

- At most one instance of the multipass daemon can run at any time. Before launching one, make sure you have stopped all other instances, including the officially installed daemon.
- On Windows, you need to run `multipassd /install` as administrator before using one of the clients. Otherwise, an authentication error will be raised.
- On Windows, the `Debug daemon` configuration will not work in an IDE session without elevated privileges. We recommend developing from an isolated environment, such as a VM or a container, if you want to run Visual Studio Code as administrator.

### Running tests

Four test suites are currently available in the project.
If you have installed the `ms-vscode.cmake-tools` extension, they should appear in the `Testing` tab of Visual Studio Code. From there, you can run or debug them.
