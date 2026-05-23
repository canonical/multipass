(how-to-guides-customise-multipass-use-multipass-content-interface)=
# Use Multipass content interface

The Multipass snap provides a content interface that allows other snaps to access its executables. This is useful for tools that need to interact with Multipass from within a strictly confined snap environment.

## What's Exposed

The `multipass-bin` slot exposes:
- `/snap/multipass/current/bin/multipass` - The Multipass CLI client
- `/snap/multipass/current/bin/multipassd` - The Multipass daemon binary

## How to Use It

### 1. In Your snapcraft.yaml

Add a plug to consume the multipass-bin content:

```yaml
plugs:
  multipass-bin:
    interface: content
    content: multipass-executables
    target: $SNAP/multipass-bin
    default-provider: multipass
```

### 2. Connect the Interface

After installing both snaps:

```bash
sudo snap connect your-snap:multipass-bin multipass:multipass-bin
```

### 3. Access the Multipass Binary

The multipass binary will be available at `$SNAP/multipass-bin/multipass`:

```bash
#!/bin/bash
# Example: Call multipass info from your snap
$SNAP/multipass-bin/multipass info
```

## Example: multipass-exporter Snap

Here's a complete example for a Prometheus exporter that needs to run `multipass info`:

```yaml
name: multipass-exporter
base: core22
confinement: strict

apps:
  exporter:
    command: bin/multipass-exporter
    daemon: simple
    plugs:
      - network
      - network-bind
      - multipass-bin

plugs:
  multipass-bin:
    interface: content
    content: multipass-executables
    target: $SNAP/multipass-bin
    default-provider: multipass

parts:
  exporter:
    plugin: go
    source: .
    build-packages:
      - golang-go
```

Then in your exporter code:

```go
package main

import (
    "os"
    "os/exec"
    "path/filepath"
)

func getMultipassInfo() ([]byte, error) {
    snapPath := os.Getenv("SNAP")
    multipassBin := filepath.Join(snapPath, "multipass-bin", "multipass")
    
    cmd := exec.Command(multipassBin, "info", "--format", "json")
    return cmd.Output()
}
```

## Notes

- The multipass daemon must be running for the commands to work
- Make sure to handle the case where the content interface might not be connected
- The consuming snap only gets read access to the binaries
- Environment variables may need to be set properly for multipass to connect to the daemon

## Troubleshooting

**Interface not connected?**
```bash
snap connections your-snap
```

**Permission denied?**
```bash
sudo snap connect your-snap:multipass-bin multipass:multipass-bin
```

**Binary not found?**
Check that the `$SNAP/multipass-bin` path exists after connecting the interface.
