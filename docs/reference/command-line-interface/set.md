(reference-command-line-interface-set)=
# set

> See also: [`get`](/reference/command-line-interface/get), [Settings](/reference/settings/index)

The `multipass set` command takes an argument in the form `<key>=<value>` to configure a single setting. The *key* part is a dot-separated path identifying the setting in a hierarchical settings tree. The *value* part is what it should be set to:

```{code-block} text
multipass set client.gui.autostart=false
```

To find what the configured value is at any moment, see [`get`](/reference/command-line-interface/get).

You can read more about the available settings in the [Settings](/reference/settings/index) reference page.

---

The full `multipass help set` output explains the available options:

```{code-block} text
Usage: multipass set [options] <key>[=<value>]
Set, to the given value, the configuration setting corresponding to the given key.

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

Arguments:
  keyval         A key, or a key-value pair. The key specifies a path to the
                 setting to configure. The value is its intended value. If only
                 the key is given, the value will be prompted for.
```
