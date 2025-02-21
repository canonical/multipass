(reference-settings-index)=
# Settings

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [GUI client](/reference/gui-client)

Multipass can be configured with a number of **settings** that are read and written by the [`get`](/reference/command-line-interface/get) and [`set`](/reference/command-line-interface/set) CLI commands, respectively. Some settings are also available in the [GUI client](/reference/gui-client).

Settings are organised in a tree structure, where each individual setting is identified by a unique **key** and takes on a single **value** at any given time.

## Settings keys

A **settings key** is a string in the form of a dot-separated path through the settings tree (such as `client.primary-name`). It specifies a path along the settings tree, from the root to a leaf. Thus, individual settings correspond to the leaves of the settings tree.

Conceptually, branches of the tree can be singled out with wildcards, to refer to multiple settings at once. For instance, `local.<instance-name>.*` designates the settings that affect a specific instance. Wildcards can also be used to select separate branches. For example `local.*.cpus` refers to the number of CPUs of Multipass instances.

## Settings values

A **settings value** is a string whose syntax (possible values/representations) and semantics (their interpretation) is determined by the setting in question.

Values often express common concepts (such as `true`, `false`, `42`, etc.) and are interpreted internally using the corresponding data types (such as boolean, integer, etc.). They can also be more complex (such as a key combination), but they are always specified and displayed through a string representation (for example: `Ctrl+Alt+U`).

## Available settings

At any given time, the available settings depend on the state of the system. Some settings are only available on some platforms, while daemon settings can only be accessed when the Multipass daemon itself can be reached.

Some instance properties are also exposed as settings.
> See also: {ref}`set-the-cpu-ram-or-disk-of-an-instance`

The command `multipass get --keys` shows what settings are available at any given time.

As of now, this is the total set of settings available:

- [`client.apps.windows-terminal.profiles`](client-apps-windows-terminal-profiles)
- [`client.primary-name`](client-primary-name)
- [`local.<instance-name>.bridged`](local-instance-name-bridged)
- [`local.<instance-name>.cpus`](local-instance-name-cpus)
- [`local.<instance-name>.disk`](local-instance-name-disk)
- [`local.<instance-name>.memory`](local-instance-name-memory)
- [`local.<instance-name>.<snapshot-name>.name`](local-instance-name-snapshot-name-name)
- [`local.<instance-name>.<snapshot-name>.comment`](local-instance-name-snapshot-name-comment)
- [`local.bridged-network`](local-bridged-network)
- [`local.driver`](local-driver)
- [`local.passphrase`](local-passphrase)
- [`local.privileged-mounts`](local-privileged-mounts)

```{caution}
Starting from Multipass version 1.14, the following settings have been removed from the CLI and are only available in the [GUI client](/reference/gui-client):
- [`client.gui.autostart`](client-gui-autostart)
- [`client.gui.hotkey`](client-gui-hotkey)
```

<!-- keeping the documentation pages for now, since not everyone will update their Multipass package immediately -->

```{toctree}
:hidden:
:titlesonly:
:maxdepth: 2
:glob:

*
