# Configure Multipass’s default logging level
> See also: [Logging levels](/reference/logging-levels)

This document demonstrates how to configure the default logging level of the Multipass service. Changing the logging level can be useful, for example, if you want to decrease the size of logging files or get more detailed information about what the daemon is doing. Logging levels can be set to one of the following: `error`, `warning`, `info`, `debug`, or `trace`, with case sensitivity.

## Changing the default logging level

[tabs]

[tab version="Linux"]

First, stop the Multipass daemon:

```bash
sudo snap stop multipass
```

After that, create the override config file, replacing `<level>` with your desired logging level:

```bash
sudo mkdir /etc/systemd/system/snap.multipass.multipassd.service.d/
sudo tee /etc/systemd/system/snap.multipass.multipassd.service.d/override.conf <<EOF
[Service]
ExecStart=
ExecStart=/usr/bin/snap run multipass.multipassd --verbosity <level>
EOF
sudo systemctl daemon-reload
```

Finally, start the Multipass daemon:

```bash
sudo snap start multipass
```

[/tab]

[tab version="macOS"]

First, become `root`:

```bash
sudo su
```

Stop the Multipass daemon:

```bash
launchctl unload /Library/LaunchDaemons/com.canonical.multipassd.plist
```

Then, open `/Library/LaunchDaemons/com.canonical.multipassd.plist` in your favorite [text editor](https://www.google.com/search?q=vi) and edit the path `/dict/array/string[2]` from `debug` to the logging level of your choice.

Finally, start the Multipass daemon:

```bash
launchctl load /Library/LaunchDaemons/com.canonical.multipassd.plist
```

[/tab]

[tab version="Windows"]

First, open an administrator privileged PowerShell prompt.

Stop the Multipass service:

```powershell
Stop-Service Multipass
```

Then, edit the Multipass service registry key with the following command:

```powershell
Set-ItemProperty -path HKLM:\System\CurrentControlSet\Services\Multipass -Name ImagePath -Value "'C:\Program Files\Multipass\bin\multipassd.exe' /svc --verbosity <level>"
```

Replacing `<level>` with your desired logging level.

Finally, start the Multipass service:

```powershell
Start-Service Multipass
```

[/tab]

[/tabs]

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/configure-multipass-default-logging-level" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

