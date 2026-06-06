# Spec: `multipass config` Command

**Reference:** [Issue #756](https://github.com/canonical/multipass/issues/756)
**Date:** 2026-05-13

---

## Overview

Add a `multipass config` top-level command that opens all Multipass settings in the user's default
text editor as a nested YAML file. When the editor closes, changes are applied via the existing
settings infrastructure. The command complements `get`/`set` by providing a discoverable,
all-in-one editing experience.

---

## Decisions and Constraints

| Topic | Decision |
|---|---|
| Format | Nested YAML (yaml-cpp, already linked) |
| Data retrieval | `MP_SETTINGS.keys()` + `MP_SETTINGS.get()` in the client; routing to daemon is transparent |
| Scope | All settings: global + per-instance + per-snapshot (excluding passphrase) |
| Apply | `MP_SETTINGS.set()` per key in the client; no transactionality required |
| Missing keys | No-op — keys absent from the edited file are not changed |
| Unknown keys | `MP_SETTINGS.set()` throws; error propagates and aborts the command |
| Errors | Let through to abort the command; no retry loop |
| No changes | Client always sends; `MP_SETTINGS.set()` handles the no-op |
| Daemon not running | Fail immediately with the standard "cannot connect" error |
| Flags | None for the initial implementation |
| Non-interactive mode | Out of scope |
| Temp file | System temp dir, `.yaml` extension, auto-deleted on exit |
| Editor | `$VISUAL` → `$EDITOR` → OS default for `.yaml` (xdg-open / open / start) |
| Passphrase | Omitted from the YAML entirely; managed via `multipass set local.passphrase` |

---

## YAML Format

Settings are grouped by top-level namespace. Dot-notation keys are nested as objects.
Instances appear as sub-objects under `local`; snapshots as sub-objects under their instance.
All values are strings.

```yaml
client:
  primary-name: primary

local:
  driver: qemu
  privileged-mounts: "false"
  bridged-network: ""
  image:
    mirror: ""
  my-vm:
    cpus: "2"
    memory: 2.0GiB
    disk: 10.0GiB
    bridged: "false"
    my-snapshot:
      name: my-snapshot
      comment: ""
  another-vm:
    cpus: "4"
    memory: 4.0GiB
    disk: 25.0GiB
    bridged: "false"
```

### Serialisation rule

To build the YAML from a flat list of dotted keys, split each key on `.` and insert into a
`YAML::Node` tree. Example: `local.image.mirror` → `root["local"]["image"]["mirror"]`.
The passphrase key (`local.passphrase`, defined as `mp::passphrase_key` in
`include/multipass/constants.h`) is skipped.

### Deserialisation rule

To flatten the edited YAML back to dotted key-value pairs, recurse the node tree:
- Scalar leaf → emit `prefix + "." + key: value` (strip leading `.` at top level).
- Map node → recurse with `prefix + "." + key`.
Only keys present in the edited YAML are included (absent = no-op).

---

## Files to Create

### `src/client/cli/cmd/config.h`

Follow the pattern of `src/client/cli/cmd/get.h`:

```cpp
#pragma once

#include <multipass/cli/command.h>
#include <multipass/rpc/multipass.grpc.pb.h>

namespace multipass::cmd
{
class Config final : public Command
{
public:
    using Command::Command;
    ReturnCodeVariant run(ArgParser* parser) override;
    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ParseCode parse_args(ArgParser* parser);
};
} // namespace multipass::cmd
```

### `src/client/cli/cmd/config.cpp`

```
Run flow:
  1. parse_args() — no positional args, no flags.
  2. Build YAML from current settings:
       a. Call MP_SETTINGS.keys() to get all setting keys.
       b. For each key, skip mp::passphrase_key.
       c. Call MP_SETTINGS.get(key) for each remaining key.
       d. Split each dotted key on '.' and insert into a YAML::Node tree.
       e. Emit the tree with YAML::Emitter.
  3. Write YAML string to a QTemporaryFile:
       QTemporaryFile tmp{QDir::tempPath() + "/multipass-config-XXXXXX.yaml"};
       tmp.setAutoRemove(true);
       tmp.open();
       tmp.write(yaml.c_str());
       tmp.flush();
       tmp.close();
  4. Resolve editor:
       - Check qEnvironmentVariable("VISUAL")
       - Else check qEnvironmentVariable("EDITOR")
       - Else use platform default:
           Linux:   "xdg-open"
           macOS:   "open"
           Windows: "cmd.exe /c start"
  5. Launch editor and wait for it to exit:
       Use QProcess::execute(editor, {tmp.fileName()}) for named editors.
       For xdg-open/open/start (which detach immediately), prefer
       QProcess::startDetached and then prompt the user to press Enter when done,
       OR use system(editor + " " + path) which blocks for terminal editors.
       Note: terminal editors (vim, nano) require the process to inherit stdin/stdout;
       use std::system() for the blocking case so the TTY is inherited.
  6. Read the edited file back:
       tmp.open(QIODevice::ReadOnly);
       auto edited_yaml = tmp.readAll().toStdString();
  7. Flatten the edited YAML to dotted key-value pairs (recurse YAML::Node tree).
  8. For each pair, call MP_SETTINGS.set(key, value).
     Any exception propagates immediately and aborts the command.
  9. Delete temp file (setAutoRemove(true) handles this) and return ReturnCode::Ok.
```

Include headers:
```cpp
#include "config.h"
#include <multipass/cli/argparser.h>
#include <multipass/constants.h>
#include <multipass/settings/settings.h>
#include <yaml-cpp/yaml.h>
#include <QDir>
#include <QProcess>
#include <QTemporaryFile>
```

---

## Files to Modify

### 1. `src/client/cli/client.cpp`

Add the include (alphabetically with existing includes, around line 39):
```cpp
#include "cmd/config.h"
```

Register the command in the constructor (alphabetically near `get` and `set`, around line 99):
```cpp
add_command<cmd::Config>();
```

---

### 2. `src/client/cli/cmd/CMakeLists.txt`

Add `config.cpp` to the `add_library(commands STATIC ...)` block, alphabetically between existing
entries (around line 26, before `get.cpp`). `yaml-cpp::yaml-cpp` is already linked at line 68 —
no change needed there.

---

## Key Reference Files

| File | Purpose | Key lines |
|---|---|---|
| `src/client/cli/cmd/get.cpp` | Pattern for command `run()`/`parse_args()` | Full file |
| `src/client/cli/cmd/set.cpp` | Pattern for command `run()`/`parse_args()` | Full file |
| `include/multipass/constants.h` | `passphrase_key` and other key constants | 61–68 |
| `include/multipass/settings/settings.h` | `MP_SETTINGS` macro, `keys()`, `get()`, `set()` | Full file |
| `include/multipass/yaml_node_utils.h` | yaml-cpp include and `YAML::Node` usage | 20–42 |
| `src/utils/yaml_node_utils.cpp` | `emit_yaml()` using `YAML::Emitter` | 82–100 |

---

## Out of Scope (Initial Implementation)

- `--instance <name>` flag to scope output to a single instance
- `--no-defaults` flag to hide keys at their default value
- `--dump` / non-interactive mode (print YAML to stdout)
- Deletion semantics (removing a key resets it to default)
- `local.passphrase` in the config editor
