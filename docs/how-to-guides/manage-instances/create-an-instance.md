(how-to-guides-manage-instances-create-an-instance)=
# Create an instance

> See also: [`launch`](reference-command-line-interface-launch), [Instance](explanation-instance)

This document demonstrates various ways to create an instance with Multipass. While every method is a one-liner involving the command `multipass launch`, each showcases a different option that you can use to get exactly the instance that you want.

## Create an instance

> See also: [`launch`](reference-command-line-interface-launch), [`info`](reference-command-line-interface-info)

To create an instance with Multipass, run the command `multipass launch`. This launches a new instance, which is randomly named; for example:

```{code-block} text
...
Launched: keen-yak
```

In particular, when we run `multipass info keen-yak`, we find out that it is an Ubuntu LTS release, namely 18.04, with 1GB RAM, 1 CPU, 5GB of disk:

```{code-block} text
Name:           keen-yak
State:          RUNNING
IPv4:           10.140.94.253
Release:        Ubuntu 18.04.1 LTS
Image hash:     d53116c67a41 (Ubuntu 18.04 LTS)
CPU(s):         1
Load:           0.00 0.12 0.18
Disk usage:     1.1G out of 4.7G
Memory usage:   71.6M out of 985.4M
```

## Create an instance with a specific image

> See also: [`find`](reference-command-line-interface-find), [`launch <image>`](reference-command-line-interface-launch), [`info`](reference-command-line-interface-info)

To find out which images are available, run `multipass find`. Here's a sample output:

```{code-block} text
Image                       Aliases           Version          Description
20.04                       focal             20240821         Ubuntu 20.04 LTS
22.04                       jammy             20240912         Ubuntu 22.04 LTS
24.04                       noble,lts         20240911         Ubuntu 24.04 LTS

Blueprint                   Aliases           Version          Description
anbox-cloud-appliance                         latest           Anbox Cloud Appliance
charm-dev                                     latest           A development and testing environment for charmers
docker                                        0.4              A Docker environment with Portainer and related tools
jellyfin                                      latest           Jellyfin is a Free Software Media System that puts you in control of managing and streaming your media.
minikube                                      latest           minikube is local Kubernetes
ros-noetic                                    0.1              A development and testing environment for ROS Noetic.
ros2-humble                                   0.1              A development and testing environment for ROS 2 Humble.
```

To launch an instance with a specific image, include the image name or alias in the command, for example `multipass launch jammy`:

```{code-block} text
...
Launched: tenacious-mink
```

`multipass info tenacious-mink` confirms that we've launched an instance of the selected image.

```{code-block} text
Name:           tenacious-mink
State:          Running
Snapshots:      0
IPv4:           192.168.64.22
Release:        Ubuntu 22.04.5 LTS
Image hash:     e898c1c93b32 (Ubuntu 22.04 LTS)
CPU(s):         1
Load:           0.00 0.02 0.01
Disk usage:     1.6GiB out of 4.8GiB
Memory usage:   149.5MiB out of 962.2MiB
Mounts:         --
```

## Create an instance with a custom name

> See also: [`launch --name`](reference-command-line-interface-launch)

To launch an instance with a specific name, add the `--name` option to the command line; for example `multipass launch kinetic --name helpful-duck`:

```{code-block} text
...
Launched: helpful-duck
```

## Create an instance with custom CPU number, disk and RAM

> See also: [`launch --cpus --disk --memory`](reference-command-line-interface-launch)

You can specify a custom number of CPUs, disk and RAM size using the `--cpus`, `--disk` and `--memory` arguments, respectively. For example:

```{code-block} text
multipass launch --cpus 4 --disk 20G --memory 8G
```

## Create an instance with primary status

> See also: [`launch --name primary`](reference-command-line-interface-launch)

An instance can obtain the primary status at creation time if its name is `primary`:

```{code-block} text
multipass launch kinetic --name primary
```

For more information, see [How to use the primary instance](how-to-guides-manage-instances-use-the-primary-instance).
