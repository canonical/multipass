# Run a Docker container in Multipass
<!-- This is published as an Ubuntu Tutorial at this link: https://ubuntu.com/tutorials/running-a-container-with-the-docker-workflow-in-multipass -->

<!--
| Key | Value |
| --- | --- |
| Summary | Running a Docker Container in Multipass |
| Categories | multipass |
| Difficulty | 2 |
| Author | nathan.hart@canonical.com |
-->

## Overview

Multipass has a Docker blueprint that gives its users access to out-of-the-box Docker on any platform. This new blueprint makes it easy to develop and test Docker containers locally on macOS, Windows, or Linux.

In this tutorial, you will see how to get started with the Docker blueprint by creating a blog in a Docker container in Multipass.

### What you’ll learn

- How to use Docker on macOS or Windows with Multipass

- How to alias the `docker` command to your host command line

- How to use [Portainer](https://www.portainer.io/) to launch a Docker container in [Multipass](http://multipass.run)

### What  you’ll need

- Any computer with an internet connection

## Install Multipass

Duration: 3 minutes

Let's start by installing Multipass on your machine, following the steps in [How to install Multipass](/how-to-guides/install-multipass). 

![|720x643](https://assets.ubuntu.com/v1/25ca03d0-mp-docker.png)

## Launch a Docker VM

Duration: 1 minute

Now that Multipass is installed, you can create a VM running Docker very simply. Open up a terminal and type

```plain
multipass launch docker
```

This command will create a virtual machine running the latest version of Ubuntu, with Docker and Portainer installed. You can now use Docker already! Try the command below to see for yourself!

```plain
multipass exec docker docker`
```

![|720x540](https://assets.ubuntu.com/v1/29e87039-mp-docker-2.png)

## Alias of the Docker commands

Duration: 1 minute

The Docker blueprint creates automatically two aliases, that is, two commands which can be run from the host to use commands in the instance as if they were in the host. In particular, the host `docker` command executes `docker` in the instance, and the host `docker-compose` command executes `docker-compose` in the instance. 

In order for these to work, you just need to add them to the path so that you can use them directly from your command line. If this was not done before, launching the Docker blueprint will return instructions showing how to add the aliases to your path. Simply copy and paste the command shown. It will likely be of this form:

```plain
PATH="$PATH:/home/<user>/snap/multipass/common/bin"
```

<!--![|720x239](https://assets.ubuntu.com/v1/2eec7028-mp-docker-3.png)-->

Run the command:

```plain
multipass launch docker
```

Sample output:

```plain
You'll need to add this to your shell configuration (.bashrc, .zshrc or so) for
aliases to work without prefixing with `multipass`:

PATH="$PATH:/home/nhart/snap/multipass/common/bin"
```

You can now use `docker` straight from the command line. To try it out, run

```plain
docker run hello-world
```

## Using Portainer

Duration: 5 minutes

Let's now go one step further, with Portainer. The Docker blueprint comes with Portainer installed, which gives an easy-to-use graphical interface for managing your Docker containers. To access Portainer, you will first need its IP address. The following command will show the IP addresses associated with the Ddocker VM you created in the previous steps:

```plain
multipass list
```

![|720x188](https://assets.ubuntu.com/v1/1e998c4e-mp-docker-4.png)

There should be two IP addresses listed, one for the Docker instance, the other for Portainer. The Portainer IP should start with a 10.

In a web browser, enter the Portainer IP address from the previous step followed by the Portainer port, 9000, like this: “<IP address>:9000”. Set up a username and password at the prompt, then select the option for managing a *local* Docker environment and click *connect*.

![|720x596](https://assets.ubuntu.com/v1/0f980233-mp-docker-5.png)

Click on the newly created “Local” environment to manage the Docker instance on your local VM.

![|720x459](https://assets.ubuntu.com/v1/3a7af624-mp-docker-6.png)

## Launching a container

Duration: 5 minutes

For this tutorial, you will be creating a blog using the Ghost template in Portainer. Portainer has many other app templates if you are looking for more ideas. If you want more selection, you can launch containers from the Docker hub from Portainer or from the command line.

Inside Portainer, click on **App Templates** in the left toolbar, and scroll down to the **Ghost** template.

![|720x461](https://assets.ubuntu.com/v1/b80ef240-mp-docker-7.png)

Now, you can configure and deploy the template. Enter a name and click deploy. The **bridge** network is the default and correct option.

![|720x541](https://assets.ubuntu.com/v1/1ade4cfc-mp-docker-8.png)

On the **Containers** page, you should now see two containers running. One containing Ghost, and the other containing Portainer itself.

![|720x373](https://assets.ubuntu.com/v1/0e720c25-mp-docker-9.png)

You can now access your Ghost blog by going to the published port indicated in the Containers page, i.e., **\<VM IP Address\>:\<Ghost Port\>**.

![|720x603](https://assets.ubuntu.com/v1/357843ef-mp-docker-10.png)

There it is, your blog running within a Docker container inside Multipass!

For next steps, try out Portainer’s other App Templates (Step 5), or check out [Docker Hub](https://hub.docker.com/search?type=image) for more containers to try. If you want to try out container orchestration, [Microk8s](https://microk8s.io/) or Multipass’ [Minikube](https://minikube.sigs.k8s.io/docs/) blueprint are great places to start.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/docker-how-to" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

