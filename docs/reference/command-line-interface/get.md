(reference-command-line-interface-get)=
# get

> See also: [`set`](/reference/command-line-interface/set), [Settings](/reference/settings/index)

The `get` command retrieves the value of a single setting specified by a key argument. This key takes the form of a dot-separated path in a hierarchical settings tree; for example:

```{code-block} text
multipass get client.gui.autostart
```

You can use the `--keys` option to get a list of all available settings keys:

```{code-block} text
multipass get --keys
```

Sample output:

```{code-block} text
client.primary-name
local.bridged-network
local.driver
local.image.mirror
local.passphrase
local.privileged-mounts
```

To set the value of a particular setting, see [`set` command](/reference/command-line-interface/set).

You can read more about the available settings in the [Settings](/reference/settings/index) reference page.

---

The full `multipass help get` output explains the available options:

```{code-block} text
Usage: multipass get [options] [<arg>]
Get the configuration setting corresponding to the given key, or all settings if no key is specified.
(Support for multiple keys and wildcards coming...)

Some common settings keys are:
  - client.gui.autostart
  - local.driver
  - local.privileged-mounts

Use `multipass get --keys` to obtain the full list of available settings at any given time.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.
  --raw          Output in raw format. For now, this affects only the
                 representation of empty values (i.e. "" instead of "<empty>").
  --keys         List available settings keys. This outputs the whole list of
                 currently available settings keys, or just <arg>, if provided
                 and a valid key.

Arguments:
  arg            Setting key, i.e. path to the intended setting.
```
