(how-to-guides-manage-instances-use-the-docker-blueprint)=
# Use the Docker blueprint (deprecated)

```{Warning}
Blueprints are deprecated and will be removed in a future release. You can achieve similar effects with cloud-init and other launch options. Find out more at: [Launch customized instances with Multipass and cloud-init](how-to-guides-manage-instances-launch-customized-instances-with-multipass-and-cloud-init)
```

The Docker blueprint gives Multipass users an easy way to create Ubuntu instances with Docker installed. It is based on the latest LTS release of Ubuntu, and includes docker engine and [Portainer](https://www.portainer.io/). The Docker blueprint automatically aliases the `docker` and `docker-compose` commands to your host, and creates a workspace that is shared between the host and the instance.

To use the Docker blueprint, run `multipass launch docker`, which will launch an instance with default parameters.

Next, follow the instructions in the output to add the aliased command to your path, it should look something like this:

```{code-block} text
You'll need to add this to your shell configuration (.bashrc, .zshrc or so) for

aliases to work without prefixing with `multipass`:

PATH="$PATH:/home/user/snap/multipass/common/bin"
```

```{note}
Running `which docker` from your host command line should confirm that you are running Docker inside Multipass.
```

To access Portainer, run `multipass ls` and copy the IP address of the multipass instance (the first in the list), then enter it into your browser followed by a colon and Portainer’s port number, 9000 (something like this: 10.21.145.191:9000). This gives you Portainer's web interface for visually managing your containers.

You can mount files into this instance as with any Multipass instance, but the default shared workspace is an easy way to edit your `dockerfiles` and `docker-compose.yaml` files from your host. With working directory mapping, you can run the `docker-compose` command from your host inside the shared directory, and have it run within that same directory in your Multipass instance.
