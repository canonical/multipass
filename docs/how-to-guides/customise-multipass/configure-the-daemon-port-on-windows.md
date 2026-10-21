(how-to-guides-customise-multipass-configure-the-daemon-port-on-windows)=
# Configure the daemon port on Windows

On Windows, the Multipass daemon (`multipassd`) listens for gRPC connections on the loopback interface, on TCP port `25051` by default. All Multipass clients (the `multipass` CLI and the desktop app) connect to that port.

If another application is using that port, Multipass resolves the conflict by itself, both when it is installed and every time the daemon starts: it moves to the next free port, remembers it in the registry, and records the move - naming the conflicting process - in the Windows event log. Only when no free port can be found at all does the daemon fail to start, with a clear error explaining what to do. The port in use is stored in the Windows registry, under:

```{code-block} text
HKEY_LOCAL_MACHINE\SOFTWARE\Canonical\Multipass
    ServerPort (REG_DWORD)
```

(configuring-a-different-daemon-port)=
## Configuring a different port

To make Multipass listen on a different port, set `ServerPort` to the port you want and restart the service. This requires an administrator prompt:

```{code-block} powershell
reg add "HKLM\SOFTWARE\Canonical\Multipass" /v ServerPort /t REG_DWORD /d 25052 /f
net stop Multipass
net start Multipass
```

The `MULTIPASS_SERVER_ADDRESS` environment variable takes precedence over this setting for clients and, on Windows, for the daemon itself. An address pinned this way - or with the daemon's `--address` option - is checked when the daemon starts: if its port is taken, the daemon refuses to start with an error naming the conflicting process, rather than silently moving away from the chosen address.

```{note}
Removing the `ServerPort` registry value makes Multipass fall back to the default port (`25051`) again. The value is kept when Multipass is uninstalled so that upgrades and reinstalls preserve your choice.
```

## Resolving port conflicts

When another application takes over the daemon's port, the daemon moves to the next free one and records the conflict in the Windows event log (Application log, source `Multipass`), naming the process holding the port. You can inspect it with:

```{code-block} powershell
Get-WinEvent -ProviderName Multipass -MaxEvents 20 | Format-List TimeCreated, Message
```

To find out which application is using a port at any time:

```{code-block} powershell
Get-NetTCPConnection -LocalPort 25051 -State Listen |
    Select-Object LocalAddress, LocalPort, OwningProcess,
        @{ Name = 'ProcessName'; Expression = { (Get-Process -Id $_.OwningProcess).ProcessName } }
```

You can then either stop or reconfigure that application, or [configure a different port](#configuring-a-different-daemon-port) for Multipass.
