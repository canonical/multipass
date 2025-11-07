(how-to-guides-manage-instances-use-a-blueprint)=
# Use a blueprint (removed)
```{Warning}
Blueprints have been removed. You can achieve similar effects with cloud-init and other launch options. Find out more at: [Launch customized instances with Multipass and cloud-init](how-to-guides-manage-instances-launch-customized-instances-with-multipass-and-cloud-init)
```

> See also:  {ref}`explanation-blueprint`

Blueprints provide a shortcut to initialising Multipass instances for a variety of applications.

To see what blueprints are available, run

```{code-block} text
multipass find --only-blueprints
```

> See also: [`find`](reference-command-line-interface-find)

To use a blueprint run:

```{code-block} text
multipass launch <blueprint name>
```

Launching a blueprint can take the same arguments as launching any other type of instance. If no further arguments are specified, the instance will launch with the defaults defined in the blueprint. Here’s an example of creating an instance from the Docker blueprint with a few more parameters specified:

```{code-block} text
multipass launch docker --name docker-dev --cpus 4 --memory 8G --disk 50G --bridged
```

This command will create an instance based on the Docker blueprint, with 4 CPU cores, 8GB of RAM, 50 GB of disk space, and connect that instance to the (predefined) bridged network.

Blueprints also provide a way of exchanging files between the host and the instance. For this, a folder named `multipass/<instance name>` is created in the user's home directory on the host and mounted in `<instance name>` in the user's home directory on the instance.

> See more: [`multipass launch`](reference-command-line-interface-launch)
