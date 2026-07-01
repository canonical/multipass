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

In particular, when we run `multipass info keen-yak`, we find out that it is an Ubuntu LTS release, with 1GB RAM, 1 CPU, 5GB of disk:

```{code-block} text
Name:           keen-yak
State:          Running
Zone:           zone1
Snapshots:      0
IPv4:           10.97.0.76
Release:        Ubuntu 26.04 LTS
Image hash:     dced94c031cc (Ubuntu 26.04 LTS)
CPU(s):         1
Load:           0.16 0.09 0.08
Disk usage:     2.2GiB out of 4.8GiB
Memory usage:   169.8MiB out of 950.4MiB
Mounts:         --
```

## Create an instance with a specific image

> See also: [`find`](reference-command-line-interface-find), [`launch <image>`](reference-command-line-interface-launch), [`info`](reference-command-line-interface-info)

To find out which images are available, run `multipass find`. Here's a sample output:

```{code-block} text
Image                       Aliases              Version          Description
22.04                       jammy                20260515         Ubuntu 22.04 LTS
24.04                       noble                20260518         Ubuntu 24.04 LTS
25.10                       questing             20260520         Ubuntu 25.10
26.04                       resolute,lts,ubuntu  20260520         Ubuntu 26.04 LTS
core:core16                                      current          Ubuntu Core 16
core:core18                                      current          Ubuntu Core 18
core:core20                                      current          Ubuntu Core 20
core:core22                                      current          Ubuntu Core 22
core:core24                                      current          Ubuntu Core 24
core:core26                                      current          Ubuntu Core 26
debian                      trixie               20260601         Debian Trixie
fedora                                           20260422         Fedora 44
```

To launch an instance with a specific image, include the image name or alias in the command, for example `multipass launch resolute`:

```{code-block} text
...
Launched: tenacious-mink
```

`multipass info tenacious-mink` confirms that we've launched an instance of the selected image.

```{code-block} text
Name:           tenacious-mink
State:          Running
Zone:           zone1
Snapshots:      0
IPv4:           10.97.0.76
Release:        Ubuntu 26.04 LTS
Image hash:     dced94c031cc (Ubuntu 26.04 LTS)
CPU(s):         1
Load:           0.16 0.09 0.08
Disk usage:     2.2GiB out of 4.8GiB
Memory usage:   169.8MiB out of 950.4MiB
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
