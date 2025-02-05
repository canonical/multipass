(how-to-guides-customise-multipass-set-up-a-graphical-interface)=
# Set up a graphical interface

<!-- This document combines
https://discourse.ubuntu.com/t/how-to-use-a-desktop-in-multipass/16229
https://discourse.ubuntu.com/t/how-to-use-stand-alone-windows-in-multipass/16340
-->

<!-- updated thanks to @dan-roscigno's contribution to CODA GitHub issue #128
https://github.com/canonical/open-documentation-academy/issues/128
-->

You can display the graphical desktop in various ways. In this document, we describe two options: RDP (Remote Display Protocol) and plain X11 forwarding. Other methods include VNC and running a Mir shell through X11 forwarding, as described in [A simple GUI shell for a Multipass VM](https://discourse.ubuntu.com/t/20439).

## Using RDP

The images used by Multipass do not come with a graphical desktop installed. For this reason, you will have to install a desktop environment (here we use `ubuntu-desktop` but there are as many other options as flavors of Ubuntu exist) along with the RDP server (we will use `xrdp` but there are also other options such as `freerdp`). 

To do this, first you need to log into a running Multipass instance. Start by listing your instances:

```{code-block} text
multipass list
```

Sample output:

```{code-block} text
Name                    State             IPv4             Image
headbanging-squid       Running           10.49.93.209     Ubuntu 22.04 LTS
```

Next, open a shell into the running instance:

```{code-block} text
multipass shell headbanging-squid
```

Once inside the instance, run the following commands to install `ubuntu-desktop` and `xrdp`:

```{code-block} text
sudo apt update
sudo apt install ubuntu-desktop xrdp
```

Now we need a user with a password to log in. One possibility is setting a password for the default `ubuntu` user:

```{code-block} text
sudo passwd ubuntu
```

You will be asked to enter and re-enter a password. 

You are done on the server side!

Quit the Ubuntu shell on the running instance with the `exit` command, and take note of the IP address to connect to. You can find the instance's IP address in the output of `multipass list` from the first step above, or you can use the `multipass info` command as well.

```{code-block} text
multipass info headbanging-squid
```

Sample output:

```{code-block} text
Name:           headbanging-squid
State:          Running
Snapshots:      0
IPv4:           10.49.93.209
Release:        Ubuntu 22.04 LTS
Image hash:     2e0c90562af1 (Ubuntu 22.04 LTS)
CPU(s):         4
Load:           0.00 0.00 0.00
Disk usage:     1.8GiB out of 5.7GiB
Memory usage:   294.2MiB out of 3.8GiB
Mounts:         --
```

In this example, we will use the IP address `10.49.93.209` to connect to the RDP server on the instance.

```{note}
If the IP address of the instance is not displayed in the output of `multipass list`, you can obtain it directly from the instance, with the command `ip addr`.
```

`````{tab-set}

````{tab-item} Linux

On Linux, there are applications such as Remmina to visualize the desktop (make sure the package `remmina-plugin-rdp` is installed in your host along with `remmina`).

To directly launch the client, run the following command:

```{code-block} text
remmina -c rdp://10.49.93.209
```

The system will ask for a username (`ubuntu`) and the password set above, and then the Ubuntu desktop on the instance will be displayed.

![Logging in to the RDP server with Remmina|690x567](https://assets.ubuntu.com/v1/83c7e6d7-multipass-remmina.png)
<!-- upload://iNMPPVChbKiM2MIo7sGoHMLctcv.png -->

````

````{tab-item} macOS

To connect on MacOS, we can use the “Microsoft Remote Desktop” application, from the Mac App Store.

````

````{tab-item} Windows

On Windows, we can connect to the RDP server with the “Remote Desktop Connection” application. There, we enter the virtual machine’s IP address, set the session to XOrg and enter the username and password we created on the previuos step. 

````

`````

And we are done... a graphical desktop!

## Using X11 forwarding

It might be the case that we only want Multipass to launch one application and to see only that window, without having the need for a complete desktop. It turns out that this setup is simpler than the RDP approach, because we do not need the Multipass instance to deploy a full desktop. Instead, we can use X11 to connect the applications in the instance with the graphical capabilities of the host.

`````{tab-set}

````{tab-item} Linux

Linux runs X by default, so no extra software in the host is needed. 

On Linux, we can use authentication in X forwarding to add a bit more security. However, we will forward through SSH to avoid struggling with `xauth`. Our user in the host will log in to the Multipass instance through SSH, so that we can pass extra parameters to it. 

To make this possible, copy your public key, stored in `~/.ssh/id_rsa.pub`, to the list of authorized keys of the instance, into the file `~/.ssh/authorized_keys`. Remember to replace the instance name used in the example with yours:

```{code-block} text
multipass exec headbanging-squid -- bash -c "echo `cat ~/.ssh/id_rsa.pub` >> ~/.ssh/authorized_keys"
```

```{note}
If the file `~/.ssh/id_rsa.pub` does not exist, it means that you need to create your SSH keys. Use `ssh-keygen` to create them and then run the previous command again.
```

Check the IP address of the instance, using `multipass info headbanging-squid` Finally, log in to the instance using X forwarding using the command (replace `xx.xx.xx.xx` with the IP address obtained above):

```{code-block} text
ssh -X ubuntu@xx.xx.xx.xx
```

Test the setting running a program of your choice on the instance; for example:

```{code-block} text
sudo apt -y install x11-apps
xlogo &
```

![Xlogo on Linux|420x455](https://assets.ubuntu.com/v1/657475f0-multipass-xlogo.png)
<!-- upload://etvJU6k1tfuZ0QsKd4TZM1ogsgR.png -->

A small window containing the X logo will show up. Done!

````

````{tab-item} macOS

The first step in Mac is to make sure a X server is running. The easiest way is to install [XQuartz](https://www.xquartz.org).

Once the X server is running, the procedure for macOS is the same as for Linux.

```{note}
Note to Apple Silicon users: some applications requiring OpenGL will not work through X11 forwarding.
```

````

````{tab-item} Windows

Windows knows nothing about X, therefore we need to install an X server. Here we will use [VcXsrv](https://sourceforge.net/projects/vcxsrv/). Other options would be [Xming](http://www.straightrunning.com/XmingNotes/) (the newest versions are paid, but older versions can still be downloaded for free from their [SourceForge site](https://sourceforge.net/projects/xming/)) or installing an X server in [Cygwin](http://cygwin.com/).

The first step would be thus to install VcXsrv and run the X server through the newly created start menu entry "XLaunch". Some options will be displayed. In the first screen, select "Multiple windows" and set the display number; leaving it in -1 is a safe option. The "Next" button brings you to the "Client startup" window, where you should select "Start no client". Click "Next" to go to the "Extra settings" screen, where you should activate the option "Disable access control". When you click "Next" you will be given the option to save the settings, and finally you can start the X server. 

An icon will show up in the dock: you are done with the X server!

To configure the client (that is, the Multipass instance) you will need the host IP address; you can obtain it with the console command `ipconfig`. 

Then, start the instance and set the `DISPLAY` environment variable to the server display on the host IP (replace `xx.xx.xx.xx` with the IP address obtained above):

```{code-block} text
export DISPLAY=xx.xx.xx.xx:0.0
```

You are done, and you can now test forwarding running a program of your choice on the instance; for example:

```{code-block} text
sudo snap install firefox
firefox &
```

![Firefox running on a Multipass instance|690x388](https://assets.ubuntu.com/v1/82019ef0-multipass-windows-desktop-firefox.jpeg)
<!-- upload://iy5xIwIRyMXjYqyhefIfdDoXnAi.jpeg -->

````

`````

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/set-up-a-graphical-interface" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

---
<small>**Contributors:** @andreitoterman , @luisp , @ricab , @gzanchi @dan-roscigno </small>

