(reference-command-line-interface-images)=
# images

The `multipass images` command lists the images Multipass can use to run instances with [`launch`](/reference/command-line-interface/launch) on your system and associated version information. For example:

```{code-block} text
Image             Aliases                     Version          Description
22.04             jammy                       20261004         Ubuntu 22.04 LTS
24.04             noble                       20260926         Ubuntu 24.04 LTS
26.04             resolute,lts,ubuntu         20260927         Ubuntu 26.04 LTS
debian            trixie                      20261001         Debian 13
fedora                                        20260422         Fedora 44
```

If no arguments are provided, `multipass images` only lists images from the default remotes.
The `--all` option includes images from all available remotes.

```{code-block} text
Remote              Image             Aliases                     Version          Description
(default)           22.04             jammy                       20261004         Ubuntu 22.04 LTS
(default)           24.04             noble                       20260926         Ubuntu 24.04 LTS
(default)           26.04             resolute,lts,ubuntu         20260927         Ubuntu 26.04 LTS
(default)           debian            trixie                      20261001         Debian 13
(default)           fedora                                        20260422         Fedora 44
core                core16                                        current          Ubuntu Core 16
core                core18                                        current          Ubuntu Core 18
...
snapcraft           26.04             resolute,lts,core26         20260802         Ubuntu 26.04 LTS
snapcraft           26.10             stonking,devel              20261006         Ubuntu 26.10
```

Launch aliases, version information and a brief description are shown next to each name in the command output.

Launching an image using its name or its alias gives the same result. For example, `multipass launch 23.10`  and  `multipass launch mantic` will launch the same image.

The aliases `lts` and `devel` are dynamic, that is, they change the image they alias from time to time, depending on the Ubuntu release.

The available aliases are:
- `default` - the default image on that remote
- `lts` - the latest Long Term Support image
- `devel` - the latest [development series](https://launchpad.net/ubuntu/devel) image (only on `daily:`)
- `<codename>` - the code name of a [Ubuntu series](https://launchpad.net/ubuntu/+series)
- `<c>` - the first letter of the code name
- `<XX.YY>` - the version number of a series

The list of available images is updated periodically. The option `--force-update` forces an immediate update of the list from the servers, before showing the output.

The option `--show-unsupported` includes old Ubuntu images, which were available at some point but are not supported anymore. This means that some features of Multipass might not work on these images and no user support is given. However, they are still available for testing.

The command also supports searching through available images. For example, `multipass images resolute` returns:

```{code-block} text
Image             Aliases                     Version          Description
26.04             resolute,lts,ubuntu         20260927         Ubuntu 26.04 LTS
```

To search images within a specific remote, `<remote_name>:` can be passed as parameter.
For instance, `multipass images daily:` returns:

```{code-block} text
Image             Aliases                     Version          Description
22.04             jammy                       20261004         Ubuntu 22.04 LTS
24.04             noble                       20260926         Ubuntu 24.04 LTS
26.04             resolute,lts                20260927         Ubuntu 26.04 LTS
26.10             stonking,devel              20261006         Ubuntu 26.10
```

To list all available remotes, use `multipass images --remotes`:

```{code-block} text
Remote
core
daily
release
snapcraft
```

---

The full `multipass help images` output explains the available options:

```{code-block} text
Usage: multipass images [options] [<remote:>][<string>]
Lists available images matching <string> for creating instances from.
With no search string, lists all aliases for supported releases.

Options:
  -h, --help          Displays help on commandline options
  -v, --verbose       Increase logging verbosity. Repeat the 'v' in the short
                      option for more detail. Maximum verbosity is obtained with
                      4 (or more) v's, i.e. -vvvv.
  --remotes           List all available remotes.
  --all               Images from all remotes will be shown.
  --show-unsupported  Show unsupported cloud images as well
  --format <format>   Output list in the requested format.
                      Valid formats are: table (default), json, csv and yaml
  --force-update      Force the image information to update from the network

Arguments:
  string              An optional value to search for in [<remote:>]<string>
                      format, where <remote> is one of the available remotes. If
                      <remote> is omitted, it will search default remotes.
                      <string> can be a partial image hash or a release version,
                      codename or alias.
```
