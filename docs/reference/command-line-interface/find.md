(reference-command-line-interface-find)=
# find

The `multipass find` command without any argument lists the images and blueprints Multipass can use to run instances with [`launch`](/reference/command-line-interface/launch) on your system and associated version information. For example:

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
appliance:adguard-home                        20200812         Ubuntu AdGuard Home Appliance
appliance:mosquitto                           20200812         Ubuntu Mosquitto Appliance
appliance:nextcloud                           20200812         Ubuntu Nextcloud Appliance
appliance:openhab                             20200812         Ubuntu openHAB Home Appliance
appliance:plexmediaserver                     20200812         Ubuntu Plex Media Server Appliance

Blueprint                   Aliases           Version          Description
anbox-cloud-appliance                         latest           Anbox Cloud Appliance
charm-dev                                     latest           A development and testing environment for charmers
docker                                        0.4              A Docker environment with Portainer and related tools
jellyfin                                      latest           Jellyfin is a Free Software Media System that puts you in control of managing and streaming your media.
minikube                                      latest           minikube is local Kubernetes
ros-noetic                                    0.1              A development and testing environment for ROS Noetic.
ros2-humble                                   0.1              A development and testing environment for ROS 2 Humble.
```

The output is separated in two sections: one for the images and one for the blueprints. Restricting the output to only one of these two categories can be done, respectively, with the `--only-images` and `--only-blueprints` options.

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

The list of available images and blueprints is updated periodically. The option `--force-update` forces an immediate update of the list from the servers, before showing the output.

The option `--show-unsupported` includes old Ubuntu images, which were available at some point but are not supported anymore. This means that some features of Multipass might now work on these images and no user support is given. However, they are still available for testing.

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
  --only-images       Show only images
  --only-blueprints   Show only blueprints
  --format <format>   Output list in the requested format.
                      Valid formats are: table (default), json, csv and yaml
  --force-update      Force the image information to update from the network

Arguments:
  string              An optional value to search for in [<remote:>]<string>
                      format, where <remote> can be either ‘release’ or ‘daily’.
                      If <remote> is omitted, it will search ‘release‘ first,
                      and if no matches are found, it will then search ‘daily‘.
                      <string> can be a partial image hash or an Ubuntu release
                      version, codename or alias.
```

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/find-command" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*
