(how-to-guides-troubleshoot-access-logs)=
# Access logs

Logs are our first go-to when something goes wrong. Multipass is comprised of a daemon process (service) and the [CLI](/reference/command-line-interface/index) and [GUI](/reference/gui-client) clients, each of them reporting on their own health.

The `multipass` command accepts the `--verbose` option (`-v` for short), which can be repeated to go from the default (*error*) level through *warning*, *info*, *debug* up to *trace*.

> See also: [Logging levels](/reference/logging-levels), [Configure Multipass’s default logging level](/how-to-guides/customise-multipass/configure-multipass-default-logging-level)

We use the underlying platform's logging facilities to ensure you get the familiar behaviour wherever you are.

`````{tab-set}

````{tab-item} Linux

On Linux, [`systemd-journald`](https://www.freedesktop.org/software/systemd/man/systemd-journald.service.html) is used, integrating with the de-facto standard for this on modern Linux systems.

To access the daemon (and its child processes') logs:

```{code-block} text
journalctl --unit 'snap.multipass*'
```

The Multipass GUI produces its own logs, that can be found under `~/snap/multipass/current/data/multipass_gui/multipass_gui.log`

````

````{tab-item} macOS

On macOS, log files are stored in `/Library/Logs/Multipass`, where `multipassd.log` has the daemon messages. You will need `sudo` to access it.

The Multipass GUI produces its own logs, that can be found under `~/Library/Application\ Support/com.canonical.multipassGui/multipass_gui.log`

````

````{tab-item} Windows

On Windows, the Event system is used and Event Viewer lets you access them. Our logs are currently under "Windows Logs/Application", where you can filter by "Multipass" Event source. You can then export the selected events to a file.

Logs from the installation and uninstall process can be found under `%TEMP%`. Sort the contents of the directory by "Date Modified" to bring the newest files to the top. The name of the file containing the logs follows the pattern `MSI[0-9a-z].LOG`.

The Multipass GUI produces its own logs, that can be found under `%APPDATA%\com.canonical\Multipass GUI\multipass_gui.log`

````

`````
