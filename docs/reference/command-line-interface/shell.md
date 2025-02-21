(reference-command-line-interface-shell)=
# shell

The `multipass shell` command will open a shell prompt on an instance. Without any arguments, it will open the shell prompt of the {ref}`primary-instance` (and also create it, if it doesn't exist). You can also pass the name of an existing instance. If the instance is not running, it will be started automatically.

If you run `multipass shell` you'll find yourself in the *primary* instance:

```{code-block} text
Launched: primary
Mounted '/Users/giulia' into 'primary:Home'
Welcome to Ubuntu 24.04.1 LTS (GNU/Linux 6.8.0-44-generic aarch64)

 * Documentation:  https://help.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/pro

 System information as of Wed Sep 18 15:11:15 UTC 2024

  System load:             0.47
  Usage of /:              40.2% of 3.80GB
  Memory usage:            22%
  Swap usage:              0%
  Processes:               109
  Users logged in:         0
  IPv4 address for enp0s1: 192.168.64.21
  IPv6 address for enp0s1: fdec:5598:6ba4:a940:5054:ff:fe88:498b


Expanded Security Maintenance for Applications is not enabled.

23 updates can be applied immediately.
23 of these updates are standard security updates.
To see these additional updates run: apt list --upgradable

Enable ESM Apps to receive additional future security updates.
See https://ubuntu.com/esm or run: sudo pro status
```

To open a shell in an existing instance, run the command:

```{code-block} text
multipass shell <instance name>
```

---

The full `multipass help shell` output explains the available options:

```{code-block} text
Usage: multipass shell [options] [<name>]
Open a shell prompt on the instance.

Options:
  -h, --help           Displays help on commandline options
  -v, --verbose        Increase logging verbosity. Repeat the 'v' in the short
                       option for more detail. Maximum verbosity is obtained
                       with 4 (or more) v's, i.e. -vvvv.
  --timeout <timeout>  Maximum time, in seconds, to wait for the command to
                       complete. Note that some background operations may
                       continue beyond that. By default, instance startup and
                       initialisation is limited to 5 minutes each.

Arguments:
  name                 Name of the instance to open a shell on. If omitted,
                       'primary' (the configured primary instance name) will be
                       assumed. If the instance is not running, an attempt is
                       made to start it (see `start` for more info).
```
