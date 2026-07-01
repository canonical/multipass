(tutorial-index)=
# Tutorial

<!-- Merged contributions by @bagustris and @pitifulpete on the Open Documentation Academy -->

Multipass is a flexible and powerful tool that can be used for many purposes. In its simplest form, you can use it to quickly create and destroy Ubuntu VMs (instances) on any host machine. But you can also use Multipass to build a local mini-cloud on your laptop, to test and develop multi-instance or container-based cloud applications.

This tutorial will help you understand how Multipass works, and the skills you need to use its main features.

## Install Multipass

Multipass is available for Linux, macOS and Windows. To install it on the OS of your choice, please follow the instructions provided in [How to install Multipass](/how-to-guides/install-multipass).

```{note}
Select the tab corresponding to your operating system (e.g. Linux) to display the relevant content in each section. Your decision will stick until you select another OS from the drop-down menu.
```

## Create and use a basic instance

`````{tab-set}

````{tab-item} Linux
:sync: Linux

Start Multipass from the application launcher. In Ubuntu, press the super key and type "Multipass", or find Multipass in the Applications panel on the lower left of the desktop.

```{figure} /images/tutorial/mp-linux-1.png
   :width: 800px
   :alt: Find Multipass in the Applications panel
```

<!-- Original image on the Asset Manager
![|800x450](https://assets.ubuntu.com/v1/949aa05e-mp-linux-1.png)
-->

After launching the application, you should see the Multipass tray icon on the upper right section of the screen.

```{figure} /images/tutorial/mp-linux-2.png
   :width: 688px
   :alt: Multipass tray icon
```

<!--
![|688x52](https://assets.ubuntu.com/v1/5ec546da-mp-linux-2.png)
-->

Click on the Multipass icon and select **Open Shell**.

```{figure} /images/tutorial/mp-linux-2a.png
   :width: 286px
   :alt: Multipass tray icon - Select "Open shell"
```

<!--
![|286x274](https://assets.ubuntu.com/v1/3ecc5e7d-mp-linux-2a.png)
-->

Clicking this button does many things in the background:
* It creates a new virtual machine (instance) named `primary`, with 1GB of RAM, 5GB of disk, and 1 CPU. See also: {ref}`primary-instance`
* It installs the most recent Ubuntu LTS release on that instance.
* It mounts your `$HOME` directory in the instance.
* It opens a shell to the instance, announced by the command prompt `ubuntu@primary`.

<!-- note added for https://github.com/canonical/multipass/issues/3537 -->

```{caution}
If your local home folder is encrypted using `fscrypt` and you are having trouble accessing its contents when it is automatically mounted inside your primary instance, see [Mount an encrypted home folder](/how-to-guides/troubleshoot/mount-an-encrypted-home-folder).
```

You can see elements of this in the printout below:

```{code-block} text
Launched: primary
Welcome to Ubuntu 26.04 LTS (GNU/Linux 7.0.0-15-generic x86_64)

 * Documentation:  https://docs.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/pro

 System information as of Tue Jun  9 15:54:32 UTC 2026

  System load:  0.4               Processes:             123
  Usage of /:   67.7% of 3.70GB   Users logged in:       0
  Memory usage: 29%               IPv4 address for eth0: 10.97.0.49
  Swap usage:   0%


Expanded Security Maintenance for Applications is not enabled.

0 updates can be applied immediately.

Enable ESM Apps to receive additional future security updates.
See https://ubuntu.com/esm or run: sudo pro status

ubuntu@primary:~$
```

Let's test it. As you've just learnt, the previous step automatically mounted your `$HOME` directory in the instance. Use this to share data with your instance. More concretely, create a new folder called `Multipass_Files` in your `$HOME` directory:

```{figure} /images/tutorial/mp-linux-3.png
   :width: 720px
   :alt: Create new "Multipass_Files" folder
```

<!--
![|720x405](https://assets.ubuntu.com/v1/fbfc8304-mp-linux-3.png)
-->

As you can see, a `README.md` file has been added to the shared folder. Check for the folder and read the file from your new instance:

```{code-block} text
cd ./Home/Multipass_Files/
cat README.md
```

Sample output:

```{code-block} text
## Shared Folder

This folder could be a great place to keep files that need to be accessed by both your host machine and Ubuntu VM!
```

````

````{tab-item} macOS
:sync: macOS

Start Multipass from the application launcher. In macOS, open the application launcher, type "Multipass", and launch the application.

```{figure} /images/tutorial/mp-macos-1.png
   :width: 720px
   :alt: Launch the Multipass application
```

<!--
![|720x451](https://assets.ubuntu.com/v1/a4ff7fad-mp-macos-1.png)
-->

After launching the application, you should see the Multipass tray icon in the upper right section of the screen.

```{figure} /images/tutorial/mp-macos-2.png
   :width: 684px
   :alt: Multipass tray icon
```

<!--
![|684x48](https://assets.ubuntu.com/v1/42bb4ccb-mp-macos-2.png)
-->

Click on the Multipass icon and select **Open Shell**.

```{figure} /images/tutorial/mp-macos-3.png
   :width: 304px
   :alt: Multipass tray icon - Select "Open shell"
```

<!--
![|304x352](https://assets.ubuntu.com/v1/eb92083a-mp-macos-3.png)
-->

Clicking this button does many things in the background:

* It creates a new virtual machine (instance) named "primary", with 1GB of RAM, 5GB of disk, and 1 CPU.
* It installs the most recent Ubuntu LTS release on that instance.
* It mounts your `$HOME` directory in the instance.
* It opens a shell to the instance, announced by the command prompt `ubuntu@primary`.

You can see elements of this in the printout below:

```{code-block} text
Launched: primary
Welcome to Ubuntu 26.04 LTS (GNU/Linux 7.0.0-15-generic x86_64)

 * Documentation:  https://docs.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/pro

 System information as of Tue Jun  9 15:54:32 UTC 2026

  System load:  0.4               Processes:             123
  Usage of /:   67.7% of 3.70GB   Users logged in:       0
  Memory usage: 29%               IPv4 address for eth0: 10.97.0.49
  Swap usage:   0%


Expanded Security Maintenance for Applications is not enabled.

0 updates can be applied immediately.

Enable ESM Apps to receive additional future security updates.
See https://ubuntu.com/esm or run: sudo pro status

ubuntu@primary:~$
```

Let’s test it out. As you've just learnt, the previous step automatically mounted your `$HOME` directory in the instance. Use this to share data with your instance. More concretely, create a new folder called `Multipass_Files` in your `$HOME` directory.

```{figure} /images/tutorial/mp-macos-4.png
   :width: 720px
   :alt: Create new "Multipass_Files" folder
```

<!--
![|720x329](https://assets.ubuntu.com/v1/5867fb49-mp-macos-4.png)
-->

As you can see, a `README.md` file has been added to the shared folder. Check for the folder and read the file from your new instance:

```{code-block} text
ubuntu@primary:~$ cd ./Home/Multipass_Files/
ubuntu@primary:~/Home/Multipass_Files$ cat README.md
## Shared Folder

This folder could be a great place to keep files that need to be accessed by both your host machine and Ubuntu VM!

ubuntu@primary:~/Home/Multipass_Files$
```

````

````{tab-item} Windows
:sync: Windows

Start Multipass from the application launcher. Press the Windows key and type "Multipass", then launch the application.

```{figure} /images/tutorial/mp-windows-1.png
   :width: 720px
   :alt: Launch the Multipass application
```

<!--
![|720x625](https://assets.ubuntu.com/v1/50b86111-mp-windows-1.png)
-->

After launching the application, you should see the Multipass tray icon in the lower right section of the screen (you may need to click on the small arrow located there).

```{figure} /images/tutorial/mp-windows-2.png
   :width: 429px
   :alt: Multipass tray icon
```

<!--
![|429x221](https://assets.ubuntu.com/v1/8c28e82a-mp-windows-2.png)
-->

Click on the Multipass icon and select **Open Shell**.

```{figure} /images/tutorial/mp-windows-3.png
   :width: 423px
   :alt: Multipass tray icon - Select "Open shell"
```

<!--
![|423x241](https://assets.ubuntu.com/v1/33a6bf4d-mp-windows-3.png)
-->

Clicking this button does many things in the background:
* First, it creates a new virtual machine (instance) named "primary", with 1GB of RAM, 5GB of disk, and 1 CPU.
* Then, it installs the most recent Ubuntu LTS release on that instance.
* Last, it opens a shell to the instance, announced by the command prompt `ubuntu@primary`.

You can see elements of this in the printout below:

```{code-block} text
Launched: primary
Welcome to Ubuntu 26.04 LTS (GNU/Linux 7.0.0-15-generic x86_64)

 * Documentation:  https://docs.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/pro

 System information as of Tue Jun  9 15:54:32 UTC 2026

  System load:  0.4               Processes:             123
  Usage of /:   67.7% of 3.70GB   Users logged in:       0
  Memory usage: 29%               IPv4 address for eth0: 10.97.0.49
  Swap usage:   0%


Expanded Security Maintenance for Applications is not enabled.

0 updates can be applied immediately.

Enable ESM Apps to receive additional future security updates.
See https://ubuntu.com/esm or run: sudo pro status

ubuntu@primary:~$
```

Try out a few Linux commands to see what you’re working with.

```{code-block} text
ubuntu@primary:~$ free
               total        used        free      shared  buff/cache   available
Mem:          925804      203872      362484         916      359448      582120
Swap:              0           0           0
ubuntu@primary:~$ df
Filesystem       1K-blocks    Used  Available Use% Mounted on
tmpfs                92584     912      91672   1% /run
/dev/sda1          4893836 1477300    3400152  31% /
tmpfs               462900       0     462900   0% /dev/shm
tmpfs                 5120       0       5120   0% /run/lock
/dev/sda15          106858    5329     101529   5% /boot/efi
tmpfs                92580       4      92576   1% /run/user/1000
```

````

`````

Congratulations, you've got your first instance!

This instance is great for when you just need a quick Ubuntu VM, but let's say you want a more customised instance, how can you do that? Multipass has you covered there too.

```{dropdown} Optional Exercises

Exercise 1:
When you select Open Shell, what happens in the background is the equivalent of the CLI commands `multipass launch –name primary`  followed by  `multipass shell`. Open a terminal and try `multipass shell` (if you didn't follow the steps above, you will have to run the `launch` command first).

Exercise 2:
In Multipass, an instance with the name "primary" is privileged. That is, it serves as the default argument of `multipass shell` among other capabilities. In different terminal instances, check `multipass shell primary` and `multipass shell`. Both commands should give the same result.
```

## Create a customised instance

Multipass has a great feature to help you get started with creating customised instances. Open a terminal and run the `multipass find` command. The result shows a list of all images you can currently launch through Multipass.

```{code-block} text
$ multipass find
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

`````{tab-set}

````{tab-item} Linux
:sync: Linux

Launch an instance running Ubuntu 26.04 ("Resolute Raccoon") by typing the `multipass launch resolute` command:

```{code-block} text
$ multipass launch resolute
Launched: coherent-trumpetfish
```

Now, you have an instance running and it has been named randomly by Multipass. In this case, it has been named "coherent-trumpetfish".

You can check some basic info about your new instance by running the following command:

```{code-block} text
$ multipass exec coherent-trumpetfish -- lsb_release -a
No LSB modules are available.
Distributor ID:   Ubuntu
Description:      Ubuntu 26.04 LTS
Release:          26.04
Codename:         resolute
```

This tells Multipass to run the command `lsb_release -a` on the "coherent-trumpetfish" instance.

Now, launch another instance, setting its name and specific memory, disk, and CPUs by running this command:

`multipass launch lts --name ltsInstance --memory 2G --disk 10G --cpus 2`

You can confirm that the new instance has the specs you need by running `multipass info ltsInstance`.

```{code-block} text
$ multipass info ltsInstance
Name:           ltsInstance
State:          Running
IPv4:           10.110.66.139
Release:        Ubuntu 26.04 LTS
Image hash:     dced94c031cc (Ubuntu 26.04 LTS)
CPU(s):         2
Load:           1.11 0.36 0.12
Disk usage:     1.4GiB out of 9.5GiB
Memory usage:   170.4MiB out of 1.9GiB
Mounts:         --
```

````

````{tab-item} macOS
:sync: macOS

Launch an instance running Ubuntu 26.04 ("Resolute Raccoon") by typing the `multipass launch resolute` command:

```{code-block} text
$ multipass launch resolute
Launched: coherent-trumpetfish
```

Now, you have an instance running and it has been named randomly by Multipass. In this case, it has been named "coherent-trumpetfish".

You can check some basic info about your new instance by running the following command:

```{code-block} text
$ multipass exec coherent-trumpetfish -- lsb_release -a
No LSB modules are available.
Distributor ID:   Ubuntu
Description:      Ubuntu 26.04 LTS
Release:          26.04
Codename:         resolute
```

This tells Multipass to run the command `lsb_release -a` on the "coherent-trumpetfish" instance.

Now, launch another instance, setting its name and specific memory, disk, and CPUs by running this command:

`multipass launch lts --name ltsInstance --memory 2G --disk 10G --cpus 2`

You can confirm that the new instance has the specs you need by running `multipass info ltsInstance`.

```{code-block} text
$ multipass info ltsInstance
Name:           ltsInstance
State:          Running
IPv4:           10.110.66.139
Release:        Ubuntu 26.04 LTS
Image hash:     dced94c031cc (Ubuntu 26.04 LTS)
CPU(s):         2
Load:           1.11 0.36 0.12
Disk usage:     1.4GiB out of 9.5GiB
Memory usage:   170.4MiB out of 1.9GiB
Mounts:         --
```

````

````{tab-item} Windows
:sync: Windows

Launch an instance running Ubuntu 26.04 ("Resolute Raccoon") by typing the `multipass launch resolute` command:

```{code-block} text
C:\WINDOWS\system32> multipass launch resolute
Launched: coherent-trumpetfish
```

Now, you have an instance running and it has been named randomly by Multipass. In this case, it has been named "coherent-trumpetfish".

You can check some basic info about your new instance by running the following command:

```{code-block} text
C:\WINDOWS\system32> multipass exec coherent-trumpetfish -- lsb_release -a
No LSB modules are available.
Distributor ID:   Ubuntu
Description:      Ubuntu 26.04 LTS
Release:          26.04
Codename:         resolute
```

This tells Multipass to run the command `lsb_release -a` on the "coherent-trumpetfish" instance.

Now, launch another instance, setting its name and specific memory, disk, and CPUs by running this command:

`multipass launch lts --name ltsInstance --memory 2G --disk 10G --cpus 2`

You can confirm that the new instance has the specs you need by running `multipass info ltsInstance`.

```{code-block} text
C:\WINDOWS\system32> multipass info ltsInstance
Name:           ltsInstance
State:          Running
IPv4:           10.110.66.139
Release:        Ubuntu 26.04 LTS
Image hash:     dced94c031cc (Ubuntu 26.04 LTS)
CPU(s):         2
Load:           1.11 0.36 0.12
Disk usage:     1.4GiB out of 9.5GiB
Memory usage:   170.4MiB out of 1.9GiB
Mounts:         --
```

````

`````

## Manage instances

Perhaps after using the "coherent-trumpetfish" instance for a while, you decide to delete it by running the following command:

`multipass delete coherent-trumpetfish`


`````{tab-set}

````{tab-item} Linux
:sync: Linux

You have created and deleted quite a few instances. It is time to run `multipass list` to see the instances you currently have.

```{code-block} text
$ multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 26.04 LTS
coherent-trumpetfish    Deleted           --               Not Available
ltsInstance             Running           10.110.66.139    Ubuntu 26.04 LTS
```

The result shows that you have two instances running, the "primary" instance and the LTS machine with customised specs. The "coherent-trumpetfish" instance is still listed, but its state is "Deleted". You can recover this instance by running `multipass recover coherent-trumpetfish`. But for now, delete the instance permanently by running `multipass purge`. Then run `multipass list` again to confirm that the instance has been permanently deleted.

```{code-block} text
$ multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 26.04 LTS
ltsInstance             Running           10.110.66.139    Ubuntu 26.04 LTS
```

````

````{tab-item} macOS
:sync: macOS

You have created and deleted quite a few instances. It is time to run `multipass list` to see the instances you currently have.

```{code-block} text
$ multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 26.04 LTS
coherent-trumpetfish    Deleted           --               Not Available
ltsInstance             Running           10.110.66.139    Ubuntu 26.04 LTS
```

The result shows that you have two instances running, the "primary" instance and the LTS machine with customised specs. The "coherent-trumpetfish" instance is still listed, but its state is "Deleted". You can recover this instance by running `multipass recover coherent-trumpetfish`. But for now, delete the instance permanently by running `multipass purge`. Then run `multipass list` again to confirm that the instance has been permanently deleted.

```{code-block} text
$ multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 26.04 LTS
ltsInstance             Running           10.110.66.139    Ubuntu 26.04 LTS
```

````

````{tab-item} Windows
:sync: Windows

You have created and deleted quite a few instances. It is time to run `multipass list` to see the instances you currently have.

```{code-block} text
C:\WINDOWS\system32> multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 26.04 LTS
coherent-trumpetfish    Deleted           --               Not Available
ltsInstance             Running           10.110.66.139    Ubuntu 26.04 LTS
```

The result shows that you have two instances running, the "primary" instance and the LTS machine with customised specs. The "coherent-trumpetfish" instance is still listed, but its state is "Deleted". You can recover this instance by running `multipass recover coherent-trumpetfish`. But for now, delete the instance permanently by running `multipass purge`. Then run `multipass list` again to confirm that the instance has been permanently deleted.

```{code-block} text
C:\WINDOWS\system32> multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 26.04 LTS
ltsInstance             Running           10.110.66.139    Ubuntu 26.04 LTS
```

````

`````

You've now seen a few ways to create, customise, and delete an instance. It is time to put those instances to work!

## Put your instances to use: Run a simple web server

Let's see a practical example of what you can do with your Multipass instances: One way to put a Multipass instance to use is by running a local web server in it.

Return to your customised LTS instance. Take note of its IP address, which was revealed when you ran `multipass list`. Then run `multipass shell ltsInstance` to open a shell in the instance.

From the shell, you can run:

```{code-block} text
sudo apt update

sudo apt install apache2
```

Open a browser and type in the IP address of your instance into the address bar. You should now see the default Apache homepage.

<!-- This used to be platform dependent but that does not make sense... -->

```{figure} /images/tutorial/mp-apache.png
   :width: 720px
   :alt: Default Apache homepage
```

<!-- Original image on the Asset Manager
![|720x545](https://assets.ubuntu.com/v1/e106f7f9-mp-linux-4.png)
-->


Just like that, you've got a web server running in a Multipass instance!

You can use this web server locally for any kind of local development or testing. However, if you want to access this web server from the internet (for instance, a different computer), you need an instance that is exposed to the external network.

## Next steps

Congratulations! You can now use Multipass proficiently.

There's more to learn about Multipass and its capabilities. Check out our [How-to guides](/how-to-guides/index) for ideas and help with your project. In our [Explanation](/explanation/index) and [Reference](/reference/index) pages you can find definitions of key concepts, a complete CLI command reference, settings options and more.

Join the discussion on the [Multipass forum](https://discourse.ubuntu.com/c/project/multipass/21/) and let us know what you are doing with your instances!

<!-- Discourse contributors
<small>**Contributors:** @nhart, @saviq, @townsend, @andreitoterman, @tmihoc, @luisp, @ricab, @sharder996, @georgeliaojia, @mscho7969, @itecompro, @mr-cal, @sally-makin, @gzanchi, @bagustris , @pitifulpete </small>
-->

<!-- Hiding for now, this will be useful to have when we have more than one tutorial.
When that's the case, the title of the page also needs to change to "Tutorials".

```{toctree}
:hidden:
:titlesonly:
:maxdepth: 2
:glob:
```
-->
