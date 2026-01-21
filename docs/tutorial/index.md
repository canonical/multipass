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
Mounted '/home/<user>' into 'primary:Home'
Welcome to Ubuntu 22.04.1 LTS (GNU/Linux 5.15.0-57-generic x86_64)

 * Documentation:  https://help.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/advantage

  System information as of Thu Jan 26 08:06:22 PST 2023

  System load:  0.0               Processes:             95
  Usage of /:   30.2% of 4.67GB   Users logged in:       0
  Memory usage: 21%               IPv4 address for ens3: 10.110.66.242
  Swap usage:   0%

 * Strictly confined Kubernetes makes edge and IoT secure. Learn how MicroK8s
   just raised the bar for easy, resilient and secure K8s cluster deployment.

   https://ubuntu.com/engage/secure-kubernetes-at-the-edge

0 updates can be applied immediately.


The list of available updates is more than a week old.
To check for new updates run: sudo apt update

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
* It installs the most recent Ubuntu LTS release on that instance. Last, it mounts your `$HOME` directory in the instance.
* It opens a shell to the instance, announced by the command prompt `ubuntu@primary`.

You can see elements of this in the printout below:

```{code-block} text
Launched: primary
Mounted '/home/<user>' into 'primary:Home'
Welcome to Ubuntu 22.04.1 LTS (GNU/Linux 5.15.0-57-generic x86_64)

 * Documentation:  https://help.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/advantage

  System information as of Thu Jan 26 21:15:05 UTC 2023

  System load:  0.72314453125     Processes:             105
  Usage of /:   29.8% of 4.67GB   Users logged in:       0
  Memory usage: 20%               IPv4 address for ens3: 192.168.64.5
  Swap usage:   0%


0 updates can be applied immediately.


The list of available updates is more than a week old.
To check for new updates run: sudo apt update

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

Clicking this button does many things in the background. First, it creates a new virtual machine (instance) named "primary", with 1GB of RAM, 5GB of disk, and 1 CPU. Second, it installs the most recent Ubuntu LTS release on that instance. Third, it mounts your `$HOME` directory in the instance. Last, it opens a shell to the instance, announced by the command prompt `ubuntu@primary`.

You can see elements of this in the printout below:

```{code-block} text
Launched: primary
Mounted '/home/<user>' into 'primary:Home'
Welcome to Ubuntu 22.04.1 LTS (GNU/Linux 5.15.0-57-generic x86_64)

 * Documentation:  https://help.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/advantage

  System information as of Thu Jan 26 08:06:22 PST 2023

  System load:  0.0               Processes:             95
  Usage of /:   30.2% of 4.67GB   Users logged in:       0
  Memory usage: 21%               IPv4 address for ens3: 10.110.66.242
  Swap usage:   0%

 * Strictly confined Kubernetes makes edge and IoT secure. Learn how MicroK8s
   just raised the bar for easy, resilient and secure K8s cluster deployment.

   https://ubuntu.com/engage/secure-kubernetes-at-the-edge

0 updates can be applied immediately.


The list of available updates is more than a week old.
To check for new updates run: sudo apt update

ubuntu@primary:~$
```

Let’s test it out. As you've just learnt, the previous step automatically mounted your `$HOME` directory in the instance. Try out a few Linux commands to see what you’re working with.

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
:C:/Users/Scott 1048576000       0 1048576000   0% /home/ubuntu/Home
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
Image                       Aliases           Version          Description
snapcraft:core18            18.04             20201111         Snapcraft builder for Core 18
snapcraft:core20            20.04             20210921         Snapcraft builder for Core 20
snapcraft:core22            22.04             20220426         Snapcraft builder for Core 22
snapcraft:devel                               20230126         Snapcraft builder for the devel series
core                        core16            20200818         Ubuntu Core 16
core18                                        20211124         Ubuntu Core 18
core20                                        20230119         Ubuntu Core 20
core22                                        20230119         Ubuntu Core 22
18.04                       bionic            20230112         Ubuntu 18.04 LTS
20.04                       focal             20230117         Ubuntu 20.04 LTS
22.04                       jammy,lts         20230107         Ubuntu 22.04 LTS
22.10                       kinetic           20230112         Ubuntu 22.10
daily:23.04                 devel,lunar       20230125         Ubuntu 23.04
appliance:adguard-home                        20200812         Ubuntu AdGuard Home Appliance
appliance:mosquitto                           20200812         Ubuntu Mosquitto Appliance
appliance:nextcloud                           20200812         Ubuntu Nextcloud Appliance
appliance:openhab                             20200812         Ubuntu openHAB Home Appliance
appliance:plexmediaserver                     20200812         Ubuntu Plex Media Server Appliance
anbox-cloud-appliance                         latest           Anbox Cloud Appliance
charm-dev                                     latest           A development and testing environment for charmers
docker                                        0.4              A Docker environment with Portainer and related tools
jellyfin                                      latest           Jellyfin is a Free Software Media System that puts you in control of managing and streaming your media.
minikube                                      latest           minikube is local Kubernetes
```

Launch an instance running Ubuntu 22.10 ("Kinetic Kudu") by typing the `multipass launch kinetic` command.

`````{tab-set}

````{tab-item} Linux

Now, you have an instance running and it has been named randomly by Multipass. In this case, it has been named "coherent-trumpetfish".

```{code-block} text
$ multipass launch kinetic
Launched: coherent-trumpetfish
```

You can check some basic info about your new instance by running the following command:

`multipass exec coherent-trumpetfish -- lsb_release -a`

This tells multipass to run the command `lsb_release -a` on the "coherent-trumpetfish" instance.

```{code-block} text
$ multipass exec coherent-trumpetfish -- lsb_release -a
No LSB modules are available.
Distributor ID:	Ubuntu
Description:	Ubuntu 22.10
Release:		22.10
Codename:		kinetic
```

Perhaps after using this instance for a while, you decide that what you really need is the latest LTS version of Ubuntu, with a more informative name and a little more memory and disk. You can delete the "coherent-trumpetfish" instance by running the following command:

`multipass delete coherent-trumpetfish`

You can now launch the type of instance you need by running this command:

`multipass launch lts --name ltsInstance --memory 2G --disk 10G --cpus 2`

````

````{tab-item} macOS

Now, you have an instance running and it has been named randomly by Multipass. In this case, it has been named "breezy-liger".

```{code-block} text
$ multipass launch kinetic
Launched: breezy-liger
```

You can check some basic info about your new instance by running the following command:

`multipass exec breezy-liger -- lsb_release -a`

This tells Multipass to run the command `lsb_release -a` on the “breezy-liger” instance.

```{code-block} text
$ multipass exec breezy-liger -- lsb_release -a
No LSB modules are available.
Distributor ID:	Ubuntu
Description:	Ubuntu 22.10
Release:		22.10
Codename:		kinetic
```

Perhaps after using this instance for a while, you decide that what you really need is the latest LTS version of Ubuntu, with a more informative name and a little more memory and disk. You can delete the "breezy-liger" instance by running the following command:

`multipass delete breezy-liger`

You can now launch the type of instance you need by running this command:

`multipass launch lts --name ltsInstance --memory 2G --disk 10G --cpus 2`

````

````{tab-item} Windows

Now, you have an instance running and it has been named randomly by Multipass. In this case, it has been named "decorous-skate".

```{code-block} text
C:\WINDOWS\system32> multipass launch kinetic
Launched: decorous-skate
```

You can check some basic info about your new instance by running the following command:

`multipass exec decorous-skate -- lsb_release -a`

This tells Multipass to run the command `lsb_release -a` on the “decorous-skate” instance.

```{code-block} text
C:\WINDOWS\system32> multipass exec decorous-skate -- lsb_release -a
No LSB modules are available.
Distributor ID:	Ubuntu
Description:	Ubuntu 22.10
Release:		22.10
Codename:		kinetic
```

Perhaps after using this instance for a while, you decide that what you really need is the latest LTS version of Ubuntu, with a more informative name and a little more memory and disk. You can delete the "decorous-skate" instance by running the following command:

`multipass delete decorous-skate`

You can now launch the type of instance you need by running this command:

`multipass launch lts --name ltsInstance --memory 2G --disk 10G --cpus 2`

````

`````

## Manage instances

You can confirm that the new instance has the specs you need by running `multipass info ltsInstance`.

`````{tab-set}

````{tab-item} Linux

```{code-block} text
$ multipass info ltsInstance
Name:           ltsInstance
State:          Running
IPv4:           10.110.66.139
Release:        Ubuntu 22.04.1 LTS
Image hash:     3100a27357a0 (Ubuntu 22.04 LTS)
CPU(s):         2
Load:           1.11 0.36 0.12
Disk usage:     1.4GiB out of 9.5GiB
Memory usage:   170.4MiB out of 1.9GiB
Mounts:         --
```

You've created and deleted quite a few instances. It is time to run `multipass list` to see the instances you currently have.

```{code-block} text
$ multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 22.04 LTS
coherent-trumpetfish    Deleted           --               Not Available
ltsInstance             Running           10.110.66.139    Ubuntu 22.04 LTS
```

The result shows that you have two instances running, the "primary" instance and the LTS machine with customised specs. The "coherent-trumpetfish" instance is still listed, but its state is "Deleted". You can recover this instance by running `multipass recover coherent-trumpetfish`. But for now, delete the instance permanently by running `multipass purge`. Then run `multipass list` again to confirm that the instance has been permanently deleted.

```{code-block} text
$ multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 22.04 LTS
ltsInstance             Running           10.110.66.139    Ubuntu 22.04 LTS
```

````

````{tab-item} macOS

```{code-block} text
$ multipass info ltsInstance
Name:           ltsInstance
State:          Running
IPv4:           192.168.64.3
Release:        Ubuntu 22.04.1 LTS
Image hash:     3100a27357a0 (Ubuntu 22.04 LTS)
CPU(s):         2
Load:           1.55 0.44 0.15
Disk usage:     1.4GiB out of 9.5GiB
Memory usage:   155.5MiB out of 1.9GiB
Mounts:         --
```

You've created and deleted quite a few instances. It is time to run `multipass list` to see the instances you currently have.

```{code-block} text
$ multipass list
Name                    State             IPv4             Image
primary                 Running           192.168.64.5     Ubuntu 22.04 LTS
breezy-liger            Deleted           --               Not Available
ltsInstance             Running           192.168.64.3     Ubuntu 22.04 LTS
```

The result shows that you have two instances running, the "primary" instance and the LTS machine with customised specs. The "breezy-liger" instance is still listed, but its state is "Deleted". You can recover this instance by running `multipass recover breezy-liger`. But for now, delete the instance permanently by running `multipass purge`. Then run `multipass list` again to confirm that the instance has been permanently deleted.

```{code-block} text
$ multipass list
Name                    State             IPv4             Image
primary                 Running           192.168.64.5     Ubuntu 22.04 LTS
ltsInstance             Running           192.168.64.3     Ubuntu 22.04 LTS
```

````

````{tab-item} Windows

```{code-block} text
C:\WINDOWS\system32> multipass info ltsInstance
Name:           ltsInstance
State:          Running
IPv4:           172.22.115.152
Release:        Ubuntu 22.04.1 LTS
Image hash:     3100a27357a0 (Ubuntu 22.04 LTS)
CPU(s):         2
Load:           1.11 0.36 0.12
Disk usage:     1.4GiB out of 9.5GiB
Memory usage:   170.4MiB out of 1.9GiB
Mounts:         --
```

You've created and deleted quite a few instances. It is time to run `multipass list` to see the instances you currently have.

```{code-block} text
C:\WINDOWS\system32> multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 22.04 LTS
decorous-skate          Deleted           --               Not Available
ltsInstance             Running           172.22.115.152   Ubuntu 22.04 LTS
```

The result shows that you have two instances running, the "primary" instance and the LTS machine with customised specs. The "decorous-skate" instance is still listed, but its state is "Deleted". You can recover this instance by running `multipass recover decorous-skate`. But for now, delete the instance permanently by running `multipass purge`. Then run `multipass list` again to confirm that the instance has been permanently deleted.

```{code-block} text
C:\WINDOWS\system32> multipass list
Name                    State             IPv4             Image
primary                 Running           10.110.66.242    Ubuntu 22.04 LTS
ltsInstance             Running           172.22.115.152   Ubuntu 22.04 LTS
```

````

`````

You've now seen a few ways to create, customise, and delete an instance. It is time to put those instances to work!

## Put your instances to use

Let's see some practical examples of what you can do with your Multipass instances:

* {ref}`run-a-simple-web-server`
* {ref}`launch-from-a-blueprint-to-run-docker-containers`

(run-a-simple-web-server)=
### Run a simple web server

One way to put a Multipass instance to use is by running a local web server in it.

Return to your customised LTS instance. Take note of its IP address, which was revealed when you ran `multipass list`. Then run `multipass shell ltsInstance` to open a shell in the instance.

From the shell, you can run:

```{code-block} text
sudo apt update

sudo apt install apache2
```

Open a browser and type in the IP address of your instance into the address bar. You should now see the default Apache homepage.

`````{tab-set}

````{tab-item} Linux

```{figure} /images/tutorial/mp-linux-4.png
   :width: 720px
   :alt: Default Apache homepage
```

<!-- Original image on the Asset Manager
![|720x545](https://assets.ubuntu.com/v1/e106f7f9-mp-linux-4.png)
-->

````

````{tab-item} macOS

```{figure} /images/tutorial/mp-macos-5.png
   :width: 720px
   :alt: Default Apache homepage
```

<!-- Original image on the Asset Manager
![|720x545](https://assets.ubuntu.com/v1/e106f7f9-mp-macos-5.png)
-->

````

````{tab-item} Windows

```{figure} /images/tutorial/mp-windows-12.png
   :width: 720px
   :alt: Default Apache homepage
```

<!-- Original image on the Asset Manager
![|720x545](https://assets.ubuntu.com/v1/e106f7f9-mp-windows-12.png)
-->

````

`````

Just like that, you've got a web server running in a Multipass instance!

You can use this web server locally for any kind of local development or testing. However, if you want to access this web server from the internet (for instance, a different computer), you need an instance that is exposed to the external network.

(launch-from-a-blueprint-to-run-docker-containers)=
### Launch from a Blueprint to run Docker containers (deprecated)
```{Warning}
Blueprints are deprecated and will be removed in a future release. You can achieve similar effects with cloud-init and other launch options.
```
Some environments require a lot of configuration and setup. Multipass Blueprints are instances with a deep level of customisation. For example, the Docker Blueprint is a pre-configured Docker environment with a Portainer container already running.

You can launch an instance using the Docker Blueprint by running `multipass launch docker --name docker-dev`.

Once that's done, run `multipass info docker-dev` to note down the IP of the new instance.

`````{tab-set}

````{tab-item} Linux

```{code-block} text
$ multipass launch docker --name docker-dev
Launched: docker-dev
$ multipass info docker-dev
Name:           docker-dev
State:          Running
IPv4:           10.115.5.235
                172.17.0.1
Release:        Ubuntu 22.04.1 LTS
Image hash:     3100a27357a0 (Ubuntu 22.04 LTS)
CPU(s):         2
Load:           1.50 2.21 1.36
Disk usage:     2.6GiB out of 38.6GiB
Memory usage:   259.7MiB out of 3.8GiB
Mounts:         --
```

Copy the IP address starting with "10" and paste it into your browser, then add a colon and the Portainer default port, 9000. It should look like this: 10.115.5.235:9000. This will take you to the Portainer login page where you can set a username and password.

```{figure} /images/tutorial/mp-linux-5.png
   :width: 720px
   :alt: Portainer login page
```

<!-- Original image on the Asset Manager
![|720x543](https://assets.ubuntu.com/v1/75a164a1-mp-linux-5.png)
-->

From there, select **Local** to manage a local Docker environment.

```{figure} /images/tutorial/mp-linux-6.png
   :width: 720px
   :alt: Portainer - Select "Local"
```

<!-- Original image on the Asset Manager
![|720x601](https://assets.ubuntu.com/v1/ee3ff308-mp-linux-6.png)
-->

Inside the newly selected local Docker environment, locate the sidebar menu on the page and click on **App Templates**, then select **NGINX**.

```{figure} /images/tutorial/mp-linux-7.png
   :width: 720px
   :alt: Portainer - Select "App templates"
```

<!-- Original image on the Asset Manager
![|720x460](https://assets.ubuntu.com/v1/86be3eae-mp-linux-7.png)
-->

From the Portainer dashboard, you can see the ports available on nginx. To verify that you have nginx running in a Docker container inside Multipass, open a new web page and paste the IP address of your instance followed by one of the port numbers.

```{figure} /images/tutorial/mp-linux-8.png
   :width: 720px
   :alt: Welcome to nginx!
```

<!-- Original image on the Asset Manager
![|720x465](https://assets.ubuntu.com/v1/25585a03-mp-linux-8.png)
-->

````

````{tab-item} macOS

```{code-block} text
$ multipass launch docker --name docker-dev
Launched: docker-dev
$ multipass info docker-dev
Name:           docker-dev
State:          Running
IPv4:           10.115.5.235
                172.17.0.1
Release:        Ubuntu 22.04.1 LTS
Image hash:     3100a27357a0 (Ubuntu 22.04 LTS)
CPU(s):         2
Load:           1.40 0.64 0.25
Disk usage:     2.5GiB out of 38.6GiB
Memory usage:   236.4MiB out of 3.8GiB
Mounts:         --
```

Copy the IP address starting with "10" and paste it into your browser, then add a colon and the Portainer default port, 9000. It should look like this: 10.115.5.235:9000. This will take you to the Portainer login page where you can set a username and password.

```{figure} /images/tutorial/mp-macos-6.png
   :width: 720px
   :alt: Portainer login page
```

<!-- Original image on the Asset Manager
![|720x543](https://assets.ubuntu.com/v1/75a164a1-mp-macos-6.png)
-->

From there, select **Local** to manage a local Docker environment.

```{figure} /images/tutorial/mp-macos-7.png
   :width: 720px
   :alt: Portainer - Select "Local"
```

<!-- Original image on the Asset Manager
![|720x601](https://assets.ubuntu.com/v1/ee3ff308-mp-macos-7.png)
-->

Inside the newly selected local Docker environment, locate the sidebar menu on the page and click on **App Templates**, then select **NGINX**.

```{figure} /images/tutorial/mp-macos-8.png
   :width: 720px
   :alt: Portainer - Select "App Templates"
```

<!-- Original image on the Asset Manager
![|720x460](https://assets.ubuntu.com/v1/86be3eae-mp-macos-8.png)
-->

From the Portainer dashboard, you can see the ports available on nginx. To verify that you have nginx running in a Docker container inside Multipass, open a new web page and paste the IP address of your instance followed by one of the port numbers.

```{figure} /images/tutorial/mp-macos-9.png
   :width: 720px
   :alt: Welcome to nginx!
```

<!-- Original image on the Asset Manager
![|720x465](https://assets.ubuntu.com/v1/25585a03-mp-macos-9.png)
-->

````

````{tab-item} Windows

```{code-block} text
C:\WINDOWS\system32> multipass launch docker --name docker-dev
Launched: docker-dev
C:\WINDOWS\system32> multipass info docker-dev
Name:           docker-dev
State:          Running
IPv4:           10.115.5.235
                172.17.0.1
Release:        Ubuntu 22.04.1 LTS
Image hash:     3100a27357a0 (Ubuntu 22.04 LTS)
CPU(s):         2
Load:           0.04 0.17 0.09
Disk usage:     2.5GiB out of 38.6GiB
Memory usage:   283.3MiB out of 3.8GiB
Mounts:         --
```

Copy the IP address starting with "10" and paste it into your browser, then add a colon and the Portainer default port, 9000. It should look like this: 10.115.5.235:9000. This will take you to the Portainer login page where you can set a username and password.

```{figure} /images/tutorial/mp-windows-14.png
   :width: 720px
   :alt: Portainer login page
```

<!-- Original image on the Asset Manager
![|720x601](https://assets.ubuntu.com/v1/75a164a1-mp-windows-14.png)
-->

From there, select **Local** to manage a local Docker environment.

```{figure} /images/tutorial/mp-windows-15.png
   :width: 720px
   :alt: Portainer - Select "Local"
```

<!-- Original image on the Asset Manager
![|720x460](https://assets.ubuntu.com/v1/ee3ff308-mp-windows-15.png)
-->

Inside the newly selected local Docker environment, locate the sidebar menu on the page and click on **App Templates**, then select **NGINX**.

```{figure} /images/tutorial/mp-windows-16.png
   :width: 720px
   :alt: Portainer - Select "App templates"
```

<!-- Original image on the Asset Manager
![](https://assets.ubuntu.com/v1/86be3eae-mp-windows-16.png)
-->

From the Portainer dashboard, you can see the ports available on nginx. To verify that you have nginx running in a Docker container inside Multipass, open a new web page and paste the IP address of your instance followed by one of the port numbers.

```{figure} /images/tutorial/mp-windows-17.png
   :width: 720px
   :alt: Welcome to nginx!
```

<!-- Original image on the Asset Manager
![|720x465](https://assets.ubuntu.com/v1/f0b28200-mp-windows-17.png)
-->

````

`````

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
