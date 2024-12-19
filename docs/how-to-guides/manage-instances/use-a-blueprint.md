# Use a blueprint
> See also: [Blueprint](/explanation/blueprint) 

Blueprints provide a shortcut to initialising Multipass instances for a variety of applications. 

To see what blueprints are available, run 

```plain
multipass find --only-blueprints
```

> See more: [`multipass find`](/reference/command-line-interface/find)

To use a blueprint run:

```plain
multipass launch <blueprint name>
```

Launching a blueprint can take the same arguments as launching any other type of instance. If no further arguments are specified, the instance will launch with the defaults defined in the blueprint. Here’s an example of creating an instance from the Docker blueprint with a few more parameters specified:

```plain
multipass launch docker --name docker-dev --cpus 4 --memory 8G --disk 50G --bridged
```

This command will create an instance based on the Docker blueprint, with 4 CPU cores, 8GB of RAM, 50 GB of disk space, and connect that instance to the (predefined) bridged network.

Blueprints also provide a way of exchanging files between the host and the instance. For this, a folder named `multipass/<instance name>` is created in the user's home directory on the host and mounted in `<instance name>` in the user's home directory on the instance.

> See more: [`multipass launch`](/reference/command-line-interface/launch)

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/use-a-blueprint" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

