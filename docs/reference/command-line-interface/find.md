(reference-command-line-interface-find)=
# find

The `multipass find` command without any argument lists the images Multipass can use to run instances with [`launch`](/reference/command-line-interface/launch) on your system and associated version information. For example:

```{code-block} text
Image                       Aliases           Version          Description
core                        core16            20200818         Ubuntu Core 16
core18                                        20211124         Ubuntu Core 18
core20                                        20230119         Ubuntu Core 20
core22                                        20230717         Ubuntu Core 22
20.04                       focal             20240129.1       Ubuntu 20.04 LTS
22.04                       jammy,lts         20240126         Ubuntu 22.04 LTS
23.10                       mantic            20240206         Ubuntu 23.10
daily:24.04                 noble,devel       20240129         Ubuntu 24.04 LTS
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

The option `--show-unsupported` includes old Ubuntu images, which were available at some point but are not supported anymore. This means that some features of Multipass might now work on these images and no user support is given. However, they are still available for testing.

The option `--only-cached` limits output to images that have already been downloaded and are stored locally. This is useful when you want to launch an instance without any network access, or when you want to know which images are ready for immediate use.

The command also supports searching through available images. For example, `multipass find mantic`  returns:

```{code-block} text
Image                       Aliases           Version          Description
mantic                                        20240206         Ubuntu 23.10
daily:mantic                                  20240206         Ubuntu 23.10
```

---

The full `multipass help find` output explains the available options:

```{code-block} text
Usage: multipass find [options] [<remote:>][<string>]
Lists available images matching <string> for creating instances from.
With no search string, lists all aliases for supported Ubuntu releases.

Options:
  -h, --help          Displays help on commandline options
  -v, --verbose       Increase logging verbosity. Repeat the 'v' in the short
                      option for more detail. Maximum verbosity is obtained with
                      4 (or more) v's, i.e. -vvvv.
  --show-unsupported  Show unsupported cloud images as well
  --format <format>   Output list in the requested format.
                      Valid formats are: table (default), json, csv and yaml
  --force-update      Force the image information to update from the network
  --only-cached       Show only locally cached images

Arguments:
  string              An optional value to search for in [<remote:>]<string>
                      format, where <remote> can be either ‘release’ or ‘daily’.
                      If <remote> is omitted, it will search ‘release‘ first,
                      and if no matches are found, it will then search ‘daily‘.
                      <string> can be a partial image hash or an Ubuntu release
                      version, codename or alias.
```
