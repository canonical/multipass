(reference-command-line-interface-find)=
# find

The `multipass find` command without any argument lists the images Multipass can use to run instances with [`launch`](/reference/command-line-interface/launch) on your system and associated version information. For example:

```{code-block} text
Image             Aliases                     Version          Description
22.04             jammy                       20260705         Ubuntu 22.04 LTS
24.04             noble                       20260705         Ubuntu 24.04 LTS
26.04             resolute,lts,ubuntu         20260720         Ubuntu 26.04 LTS
daily:26.10       stonking,devel              20260627         Ubuntu 26.10
core:core16                                   current          Ubuntu Core 16
core:core18                                   current          Ubuntu Core 18
core:core20                                   current          Ubuntu Core 20
core:core22                                   current          Ubuntu Core 22
core:core24                                   current          Ubuntu Core 24
core:core26                                   current          Ubuntu Core 26
debian            trixie                      20260706         Debian Trixie
fedora                                        20260422         Fedora 44
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

The command also supports searching through available images. For example, `multipass find resolute` returns:

```{code-block} text
Image             Aliases                     Version          Description
daily:resolute                                20260720         Ubuntu 26.04 LTS
resolute                                      20260720         Ubuntu 26.04 LTS
```

---

The full `multipass help find` output explains the available options:

```{code-block} text
Usage: multipass find [options] [<remote:>][<string>]
Lists available images matching <string> for creating instances from.
With no search string, lists all aliases for supported releases.

Options:
  -h, --help          Displays help on commandline options
  -v, --verbose       Increase logging verbosity. Repeat the 'v' in the short
                      option for more detail. Maximum verbosity is obtained with
                      4 (or more) v's, i.e. -vvvv.
  --show-unsupported  Show unsupported cloud images as well
  --format <format>   Output list in the requested format.
                      Valid formats are: table (default), json, csv and yaml
  --force-update      Force the image information to update from the network

Arguments:
  string              An optional value to search for in [<remote:>]<string>
                      format, where <remote> can be either ‘release’ or ‘daily’.
                      If <remote> is omitted, it will search ‘release‘ first,
                      and if no matches are found, it will then search ‘daily‘.
                      <string> can be a partial image hash or a release version,
                      codename or alias.
```
