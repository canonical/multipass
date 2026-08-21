# Multipass Development Guidelines for AI Coding Agents

## Architecture Overview

Multipass is a cross-platform VM manager with a **client-daemon architecture** communicating via **gRPC streaming**. The codebase follows clear separation between platform-agnostic logic and platform-specific implementations.

### Core Components

- **Daemon** (`src/daemon/`): Backend service managing VMs, authentication, and operations
- **CLI Client** (`src/client/cli/`): Command-line interface implementing subcommands in `src/client/cli/cmd/*.cpp`
- **GUI Client** (`src/client/gui/`): Flutter-based desktop application with Dart gRPC client
- **gRPC API** (`src/rpc/multipass.proto`): Streaming RPC protocol for all client-daemon communication
- **Platform Backends** (`src/platform/backends/`): Hypervisor abstractions (QEMU, Apple VZ, VirtualBox, Hyper-V)
- **Image System** (`src/image_host/`, `src/daemon/default_vm_image_vault.cpp`): Image fetching, caching, and validation
- **Utilities** (`src/utils/`, `src/network/`, `src/ssh/`, `src/sshfs_mount/`): Cross-cutting concerns

### Key Architectural Patterns

**Client-Daemon Communication**: All operations use bidirectional gRPC streams, not simple request-response. Every RPC method signature includes `ServerReaderWriter`:
```cpp
// Daemon side - streaming handler in daemon_rpc.cpp
grpc::Status DaemonRpc::launch(grpc::ServerContext* context,
                               grpc::ServerReaderWriter<LaunchReply, LaunchRequest>* server);
```

**Platform Abstraction**: Backends implement common interfaces (`VirtualMachine`, `VirtualMachineFactory`) with platform-specific behavior:
- `qemu/` - Primary backend using QEMU with QMP (QEMU Machine Protocol) for control
- `applevz/` - macOS backend using Apple's Virtualization.framework
- `virtualbox/` - VirtualBox integration via VBoxManage commands (macOS/Windows)
- `hyperv/` - Windows Hyper-V backend using PowerShell cmdlets
- `shared/` - Common base classes like `BaseVirtualMachine`, `BaseSnapshot`

**Image Vault System**: Multi-layer image management:
```cpp
// Image hosts provide sources (Ubuntu, custom URLs)
class VMImageHost { virtual optional<VMImageInfo> info_for(const Query& query) = 0; };
class UbuntuVMImageHost : public VMImageHost; // SimpleStreams protocol
class CustomVMImageHost : public VMImageHost; // Direct URL downloads

// Vault coordinates fetching, verification, caching
VMImage DefaultVMImageVault::fetch_image(const FetchType& fetch_type,
                                         const Query& query,
                                         const PrepareAction& prepare,
                                         const ProgressMonitor& monitor);
```

**Qt Integration**: Heavy use of Qt signals/slots for async operations and cross-thread communication:
```cpp
QObject::connect(vm_process.get(), &Process::started, [this]() {
    on_started();
});
```

**Cloud-Init Integration**: VMs provisioned via cloud-init ISO images attached at boot:
```cpp
// Generated per-instance in src/daemon/daemon.cpp
auto cloud_init_iso = instance_dir.filePath("cloud-init-config.iso");
mp::CloudInitIso::make_cloud_init_iso(vm_desc, key_provider, cloud_init_iso);
// Contains meta-data, user-data, vendor-data, network-config
```

## Build System & Dependencies

### CMake Structure
- **vcpkg**: Managed via git submodule for C++ dependencies (gRPC, protobuf, Qt6)
- **Flutter**: Embedded in `3rd-party/flutter/` for GUI client
- **Cross-platform builds**: Automated via GitHub Actions with matrix builds
- **Conditional compilation**: Platform-specific features controlled by CMake variables

### Critical Build Commands
```bash
# Configure with vcpkg toolchain (auto-bootstrapped)
cmake -B build -DMULTIPASS_ENABLE_FLUTTER_GUI=ON

# Build everything
cmake --build build

# Build specific targets
cmake --build build --target multipass  # CLI
cmake --build build --target multipassd  # Daemon
cmake --build build --target multipass_gui  # Flutter GUI
```

### Testing Infrastructure
- **GoogleTest/GMock**: Unit tests in `tests/` directory (FetchContent from GitHub main branch)
- **Mock Framework**: Extensive mocking infrastructure (`mock_*.h` files) - nearly every interface has a mock
- **Test Fixtures**: Common base classes like `daemon_test_fixture.h` provide consistent setup
- **Platform-specific tests**: Separate test directories (`tests/qemu/`, `tests/macos/`, `tests/linux/`, `tests/windows/`)
- **Run tests**: `ctest` or `cmake --build build --target test` after building

## Development Workflows

### Code Organization Patterns

**Error Handling**: Custom exception hierarchy with specific VM state exceptions:
```cpp
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
throw VMStateIdleException(fmt::format("Instance \"{}\" is not running", name));
```

**Logging**: Structured logging with multipass logging namespace:
```cpp
namespace mpl = multipass::logging;
mpl::info(vm_name, "Starting VM with {} cores", num_cores);
```

**Settings Management**: Centralized configuration via settings handlers:
```cpp
// Settings are persistent and validated
MP_SETTINGS.set(multipass::petname_key, new_name);
```

### VM Lifecycle Management

VMs follow a strict state machine pattern managed by `VMStatusMonitor`:
```cpp
enum class State { off, starting, restarting, running, delayed_shutdown, suspending, suspended, unknown };
```

**State Persistence**: VM metadata stored as JSON and managed by daemon:
```cpp
void update_metadata_for(const std::string& name, const QJsonObject& metadata) override;
QJsonObject retrieve_metadata_for(const std::string& name) override;
```

**Snapshot System**: Each backend implements snapshot capability via `Snapshot` interface:
- Snapshots store VM state, configuration, mounts, network interfaces, cloud-init instance ID
- Hierarchical parent-child relationships tracked with indices
- Creation timestamp and metadata preserved in QJsonObject format

### Platform-Specific Considerations

**QEMU Backend**: Uses QMP for VM control, expects JSON responses:
```cpp
vm_process->write(qmp_execute_json("qmp_capabilities"));
auto qmp_output = vm_process->read_all_standard_output();
auto qmp_object = QJsonDocument::fromJson(qmp_output.split('\n').first()).object();
```

**Process Management**: Each backend manages hypervisor processes differently:
- QEMU: Direct process spawning with QMP protocol over stdout/stdin
- VirtualBox: VBoxManage command execution via ProcessFactory
- Hyper-V: PowerShell cmdlet integration with quoted path handling

## Critical File Patterns

### Project Structure Navigation
- VM implementations: `src/platform/backends/{qemu,applevz,virtualbox,hyperv}/`
- Client commands: `src/client/cli/cmd/*.cpp` (each command is a separate file)
- gRPC service: `src/daemon/daemon_rpc.cpp` (routing) and `src/daemon/daemon.cpp` (business logic)
- Platform utilities: `src/platform/` (process specs, networking)
- Test infrastructure: `tests/` with extensive mocking (each interface has corresponding `mock_*.h`)
- Image hosts: `src/image_host/{ubuntu_image_host.cpp,custom_image_host.cpp}` using SimpleStreams protocol

### Key Interfaces
- `VirtualMachine`: Core VM abstraction all backends implement (`include/multipass/virtual_machine.h`)
- `VirtualMachineFactory`: Factory pattern for VM creation with `fetch_type()` for image requirements
- `VMStatusMonitor`: State change notification interface
- `SSHKeyProvider`: Authentication key management (pub/private key pairs)
- `MountHandler`: Filesystem mounting abstraction (SSHFS for QEMU, SMB for Windows, native for VirtualBox)
- `VMImageVault`: Image caching/validation with `fetch_image()` method
- `Snapshot`: VM state capture with hierarchical parent-child relationships

### Flutter GUI Specifics
- Dart gRPC client: `src/client/gui/lib/grpc_client.dart` with stream-based RPC handlers
- State management: Provider pattern with `providers.dart` and `providerContainer`
- Platform integration: FFI for native daemon communication (`src/client/gui/ffi/`)
- Update notifications: Responses checked for `updateInfo` field to notify users of new versions
- Logging: All gRPC requests/responses logged via `logger.i()` with privacy filters

## Testing Approaches

**Mock-Heavy Testing**: Extensive use of Google Mock for isolating components:
```cpp
MOCK_METHOD(void, start, (), (override));
MOCK_METHOD(VirtualMachine::State, current_state, (), (override));
```

**Fixture Pattern**: Common test fixtures for daemon, SSH, SFTP operations provide consistent test environment setup.

**Platform Test Isolation**: Separate test directories mirror source structure (`tests/qemu/`, `tests/macos/`) for platform-specific validation.

## Development Guidelines

1. **Always use absolute paths** in file operations and CMake targets
2. **gRPC streams are bidirectional** - handle both request and response messages
3. **Qt signal/slot connections** are primary async pattern - avoid raw threading
4. **Platform abstraction is critical** - new features must work across all supported backends
5. **State management is explicit** - VM states must be persisted and restored correctly
6. **Error propagation follows exception hierarchy** - use specific exception types, not generic errors

## Code Formatting and Linters

**C++ Code**: All C++ code must be formatted using `clang-format` with the project's `.clang-format` configuration:
```bash
# Format a single file
clang-format -i path/to/file.cpp

# Format all C++ files in a directory
find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

Key formatting rules from `.clang-format`:
- **C++20 standard**
- **4-space indentation**
- **100-character column limit**
- **No short functions on single line** - all function bodies use braces on separate lines
- **Custom brace wrapping** - braces after classes, functions, control statements on new lines
- **One parameter per line** - no bin-packing of arguments or parameters
- **Pointer alignment left** - `Type* ptr` not `Type *ptr`

**Dart Code**: All Flutter/Dart code must be formatted using `dart format`:
```bash
# Format all Dart files in the GUI client
dart format src/client/gui/lib/

# Format a single file
dart format path/to/file.dart
```

**Always run formatters** before committing code to ensure consistency with the project's style guidelines.

## Common Pitfalls

- **Don't assume synchronous operations** - most VM operations are async with state callbacks
- **Avoid direct platform APIs** - use abstraction layers in `src/platform/`
- **gRPC streaming requires proper error handling** - check status codes and handle disconnections
- **Qt objects need proper thread affinity** - signals cross thread boundaries safely
- **CMake dependencies must be explicit** - vcpkg integration requires proper target linking

## Commit Message Guidelines

All commit messages are validated by `git-hooks/commit-msg.py` and must follow these rules:

**Subject Line Format**:
1. **Begin with a category** in lowercase within square brackets: `[category]`
   - Categories are single words or hyphenated composite words (e.g., `[fix]`, `[bug-fix]`, `[feature]`)
2. **Follow with a space and capitalize** the first word after the category: `[fix] Update documentation`
3. **Limit subject to 50 characters** (including the category)
4. **No period at the end** of the subject line

**Body Format** (if present):
5. **Separate body from subject** with a blank line
6. **Wrap lines at 72 characters**, except for:
   - Blockquotes (lines starting with `>`)
   - References (e.g., `[1]: https://...`)
   - Sign-offs (`Signed-off-by:`)
   - Co-authors (`Co-authored-by:`)
7. **No consecutive blank lines** (except in quoted text)

**Examples**:
```
[fix] Update documentation for API changes
```

```
[feature] Add user authentication system

This implements a new authentication system using JWT tokens. The system
supports multiple providers and includes session management.

Key changes:
- Add JWT token generation and validation
- Implement session storage with Redis
- Add provider abstraction layer
```

**Git autosquash support**: The hook allows `fixup!` and `squash!` prefixes in non-strict mode for interactive rebase workflows.

This codebase emphasizes clean architecture separation, comprehensive testing, and cross-platform compatibility through well-defined abstraction layers.

## Code Review

When reviewing code changes or pull requests, apply the workflow and rules in `.github/skills/code-review/SKILL.md`. In short: verify every finding against the actual code before asserting it, keep comments few and high-signal, and stay silent on formatting and intentional project idioms.
