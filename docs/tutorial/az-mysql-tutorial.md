(tutorial-az-mysql-mysql)=

# High availability across Multipass availability zones with Juju and Charmed MySQL

In this tutorial, we will use Juju with Multipass virtual machines to test Charmed MySQL high availability behavior across failure domains.

This gives you a model-driven workflow for (High Availability) HA testing without manually wiring primary and replica IPs.

```{note}

Multipass supports availability zones through its `--zone` flag. On a single host, all VMs share the same physical hardware, but each VM is assigned to a named zone that simulates zones.

```

## What you'll do in this tutorial

1. Install and verify Juju and Multipass on the host.

2. Launch Multipass instances across different availability zones.

3. Add the instances to Juju and deploy Charmed MySQL with three units.

4. Confirm unit spread, replication roles, and healthy cluster state.

5. Simulate failover by disabling the zone that contains the primary MySQL VM and verify automatic recovery.

6. Tear down Juju resources and Multipass instances.


## Requirements

To complete this tutorial, you will need:

- A host with Multipass 1.17 or later.

- Permission to install Juju 3.x on your host.

> See the [Multipass installation guide](install-multipass-prerequisites) for your platform

## Install Juju on the host

If Juju is not already installed, install it before continuing.

`````{tab-set}

````{tab-item} Linux
:sync: linux

``` bash
sudo snap install juju --channel 3.6/stable --classic
```

````

````{tab-item} macOS
:sync: macos

Brew does not provide a Juju 3.x formula. Download the binary directly from GitHub:

```bash
curl -L -o juju-3.6.14.tar.xz \
  "https://github.com/juju/juju/releases/download/v3.6.14/juju-3.6.14-darwin-arm64.tar.xz"
tar xf juju-3.6.14.tar.xz
sudo mv juju /usr/local/bin/juju
```

``` {note}
This installs the `arm64` build for Apple Silicon (M series). If you are on an Intel Mac, replace `darwin-arm64` with `darwin-amd64` in the URL.
```

````
`````

Verify the installation:

```bash

juju version

```

Check charm base compatibility before deploying:

```bash

juju info mysql --channel 8.0/stable

```

In the output, confirm that `ubuntu@22.04` is listed under supported bases.

## Launch the instances

To avoid surprises when Multipass updates its default image, pin the instances to Ubuntu 22.04 (which is supported by `mysql` `8.0/stable`).

Launch three Multipass instances in different zones. These will be Juju machines:

```bash

multipass launch 22.04 --name juju-1 --zone zone1 --memory 2G --disk 5G

multipass launch 22.04 --name juju-2 --zone zone2 --memory 2G --disk 5G

multipass launch 22.04 --name juju-3 --zone zone3 --memory 2G --disk 5G

```

``` {note}

The `--zone` flag assigns each VM to a named Multipass availability zone. Each zone represents a simulated failure domain: `zone1`, `zone2`, and `zone3`. In the failover test, stopping `juju-1` simulates `zone1` going offline. Juju tracks these VMs as machines and manages MySQL placement and recovery across them — one unit per machine, each in a different zone.

```

Capture the instance IP addresses:

```bash

IP_A=$(multipass info juju-1 --format csv | awk -F, 'NR>1 {print $5}')

IP_B=$(multipass info juju-2 --format csv | awk -F, 'NR>1 {print $5}')

IP_C=$(multipass info juju-3 --format csv | awk -F, 'NR>1 {print $5}')


echo "$IP_A" , "$IP_B" , "$IP_C"

```

## Bootstrap Juju

Bootstrap a controller and create a model for testing.

`````{tab-set}



````{tab-item} Linux
:sync: linux



On Linux, `localhost` uses LXD. Make sure LXD is installed and initialized before bootstrapping.



```bash

juju bootstrap localhost mp-controller

juju add-model az-mysql-lab

```



``` {note}

`juju add-model` creates a named workspace on the **controller machine** where all Juju state for this tutorial is tracked. The controller's own built-in model is reserved for Juju infrastructure, so user workloads must go in a separate model. The name `az-mysql-lab` is descriptive — `az` for availability zones, `mysql-lab` for what will be deployed. If you skip this step and run `juju deploy` or `juju add-machine`, Juju will report "no current model" and refuse to proceed.

```



````



````{tab-item} macOS
:sync: macos



On macOS, `juju bootstrap localhost` does not work because the `localhost` cloud expects LXD. Instead, bootstrap the controller onto a dedicated Multipass instance over SSH.



**Step 1: Launch the controller instance**


If you have only launched the three workload instances so far, launch one extra instance for the controller:


``` {note}

The controller VM runs the Juju API server and database. It requires at least 4 GB RAM and 10 GB disk. A smaller instance will cause `juju bootstrap` or `juju add-model` to hang or fail silently due to memory exhaustion.

```

``` bash

multipass launch 22.04 --name juju-controller --zone zone3 --memory 4G --disk 10G

CTRL_IP=$(multipass info juju-controller --format csv | awk -F, 'NR>1 {print $5}')
echo "Controller IP: $CTRL_IP"
```



**Step 2: Ensure you have an SSH key pair**



Juju connects to each Multipass instance over SSH using your local key. Check whether a key already exists:



``` bash

ls ~/.ssh/*.pub

```



If no key is listed, generate one:



```bash

ssh-keygen -t ed25519 -N "" -f ~/.ssh/id_ed25519

```



Identify the public key file you want to use. The examples below use `~/.ssh/id_ed25519.pub`. If your key has a different name (such as `id_rsa.pub` or `id_ed25519_multipass.pub`), substitute that path in the commands below.



**Step 3: Inject your public key into every Multipass instance**



Multipass instances do not automatically trust your local SSH key. Inject it into all four instances:



``` bash

PUB_KEY=$(cat ~/.ssh/id_ed25519.pub)   # adjust filename if needed

for vm in juju-controller juju-1 juju-2 juju-3; do

  multipass exec "$vm" -- bash -c "echo '$PUB_KEY' >> /home/ubuntu/.ssh/authorized_keys"

done

```



Verify that plain SSH now works before continuing:

``` bash

ssh ubuntu@$CTRL_IP 'echo ssh-ok'

```

The output is:

``` {terminal}

The authenticity of host '192.168.2.171 (192.168.2.171)' can't be established.
ED25519 key fingerprint is: SHA256:o5CR8NNnL4AKhv7vDSvFfrHkyG+cU7c6s+1EA7Wn4X0
This key is not known by any other names.
Are you sure you want to continue connecting (yes/no/[fingerprint])? yes
Warning: Permanently added '192.168.2.171' (ED25519) to the list of known hosts.

ssh-ok
```



**Step 4: Register the manual cloud and bootstrap**



Register your Multipass VMs as a manual cloud, then bootstrap:



```bash

juju add-cloud --client

```

When prompted:

- **Cloud type**: `manual`

- **Cloud name**: `mp-manual`

- **SSH connection string**: `ubuntu@<CTRL_IP>` (for example, `ubuntu@192.168.2.162`)

- **Host key verification**: type `yes` and press Enter. Do not type the fingerprint value shown in brackets.



Then bootstrap and create the model:




```bash

juju bootstrap mp-manual

```

```{note}
This may take a minute or two as Juju uploads the agent to the controller instance, installs it, and waits for the API server to become responsive.
```

Example output when bootstrap completes successfully:



``` bash

Creating Juju controller "mp-controller" on mp-manual/default
Looking for packaged Juju agent version 3.6.14 for arm64
Located Juju agent version 3.6.14-ubuntu-arm64 at https://streams.canonical.com/juju/tools/agent/3.6.14/juju-3.6.14-linux-arm64.tgz
Installing Juju agent on bootstrap instance
Running machine configuration script...
Bootstrap agent now started
Contacting Juju controller at 192.168.2.166 to verify accessibility...

Bootstrap complete, controller "mp-controller" is now available
Controller machines are in the "controller" model

Now you can run
        juju add-model <model-name>
to create a new model to deploy workloads.

```


After bootstrap completes, create the model:


```bash

juju add-model az-mysql-lab

```

Example output:

```bash

Added 'az-mysql-lab' model on mp-manual/default with credential 'default' for user 'admin'

```



``` {note}

`juju add-model` creates a named workspace on the **controller VM** (`juju-controller`) where all Juju state for this tutorial is tracked. The controller's own built-in model is reserved for Juju infrastructure, so user workloads must go in a separate model. The name `az-mysql-lab` is descriptive — `az` for availability zones, `mysql-lab` for what will be deployed. If you skip this step and run `juju deploy` or `juju add-machine`, Juju will report "no current model" and refuse to proceed.

```



When it completes, verify the controller is registered:



```bash

juju controllers

```



Example output:
```bash

Controller      Model  User   Access     Cloud/Region       Models  Nodes    HA  Version
mp-controller*  -      admin  superuser  mp-manual/default       1      1  none  3.6.14

```

````

`````

Add your Multipass VMs as Juju machines, specifying the zone for each:

```{caution}
When you select 'yes' when prompted `Are you sure you want to continue connecting (yes/no/[fingerprint])?`, it may take a few minutes for the connection to succeed.
```

```bash

juju add-machine --constraints zones=zone1 ssh:ubuntu@$IP_A

juju add-machine --constraints zones=zone2 ssh:ubuntu@$IP_B

juju add-machine --constraints zones=zone3 ssh:ubuntu@$IP_C

```

``` {note}

For a manual (SSH) cloud, Juju connects to machines over SSH and does not query Multipass for zone metadata, so the `AZ` column in `juju machines` output will be empty. The zone topology is defined at the Multipass level — each machine belongs to a different zone — and the failover test demonstrates what happens when one of those zones goes offline.

```

Verify Juju sees the machines:

```bash

juju machines

```

Example output:

```bash

Machine  State    Address        Inst id               Base          AZ  Message
0        started  192.168.2.159  manual:192.168.2.159  ubuntu@22.04      Manually provisioned machine
1        started  192.168.2.160  manual:192.168.2.160  ubuntu@22.04      Manually provisioned machine
2        started  192.168.2.161  manual:192.168.2.161  ubuntu@22.04      Manually provisioned machine

```

## Test: Charmed MySQL spread and failover

Charmed MySQL provides clustered database behavior with automatic role management and recovery. For background, see:

> See also [Scale replicas](https://canonical-charmed-mysql.readthedocs-hosted.com/tutorial/#scale-replicas), [Primary switchover](https://canonical-charmed-mysql.readthedocs-hosted.com/how-to/primary-switchover/)

Deploy three MySQL units:

`````{tab-set}



````{tab-item} Linux
:sync: linux



```bash

juju deploy mysql --channel 8.0/stable -n 3

```



````



````{tab-item} macOS
:sync: macos



On Apple Silicon, Multipass VMs run as ARM64. You must tell Juju to use the ARM64 agent and charm:



```bash

juju deploy mysql --channel 8.0/stable -n 3 --constraints arch=arm64

```



````



`````

Watch placement and health:

```bash

juju status mysql --watch 2s

```

Deployment takes a few minutes while the charm installs MySQL on each machine. Once all units settle, the output will show the elected primary:

```bash
juju status mysql


Model         Controller     Cloud/Region       Version  SLA          Timestamp
az-mysql-lab  mp-controller  mp-manual/default  3.6.14   unsupported  00:48:26+03:00

App    Version          Status  Scale  Charm  Channel     Rev  Exposed  Message
mysql  8.0.44-0ubun...  active      3  mysql  8.0/stable  442  no

Unit      Workload  Agent  Machine  Public address  Ports           Message
mysql/0*  active    idle   0        192.168.2.159   3306,33060/tcp  Primary
mysql/1   active    idle   1        192.168.2.160   3306,33060/tcp
mysql/2   active    idle   2        192.168.2.161   3306,33060/tcp

Machine  State    Address        Inst id               Base          AZ  Message
0        started  192.168.2.159  manual:192.168.2.159  ubuntu@22.04      Manually provisioned machine
1        started  192.168.2.160  manual:192.168.2.160  ubuntu@22.04      Manually provisioned machine
2        started  192.168.2.161  manual:192.168.2.161  ubuntu@22.04      Manually provisioned machine

```

At this point, we have a healthy MySQL cluster with three units, each on a different machine and zone. The unit marked with '*' is the primary, and the others are replicas.

Below is a representationof this setup:

```{figure} /images/tutorial/az-diagram.jpg
   :width: 658px
   :alt: Multipass AZ tutorial: HA Mysql with Juju
```

### Failover check: disable zone1

Disable `zone1` to simulate the zone containing the primary MySQL VM going offline:

```bash

multipass disable-zones zone1

```

Watch recovery complete:

```bash

juju status mysql

Model         Controller     Cloud/Region       Version  SLA          Timestamp
az-mysql-lab  mp-controller  mp-manual/default  3.6.14   unsupported  00:53:16+03:00

App    Version          Status  Scale  Charm  Channel     Rev  Exposed  Message
mysql  8.0.44-0ubun...  active    2/3  mysql  8.0/stable  442  no

Unit      Workload     Agent      Machine  Public address  Ports           Message
mysql/0   unknown      lost       0        192.168.2.159   3306,33060/tcp  agent lost, see 'juju show-status-log mysql/0'
mysql/1*  maintenance  executing  1        192.168.2.160   3306,33060/tcp  joining the cluster
mysql/2   maintenance  idle       2        192.168.2.161   3306,33060/tcp  unreachable

Machine  State    Address        Inst id               Base          AZ  Message
0        down     192.168.2.159  manual:192.168.2.159  ubuntu@22.04      Manually provisioned machine
1        started  192.168.2.160  manual:192.168.2.160  ubuntu@22.04      Manually provisioned machine
2        started  192.168.2.161  manual:192.168.2.161  ubuntu@22.04      Manually provisioned machine

```

Successful failover looks like this:

- Application remains `active`.

- A different MySQL unit takes over primary duties. It is marked with an '*' in the `Unit` column and shows "Primary" in the `Message` column.
in this case:

```
mysql/1*  maintenance  idle   1        192.168.2.160   3306,33060/tcp  joining the cluster
```

- Remaining units stay healthy as peers/replicas.

## Verify the HA behavior

Use these checks after the MySQL primary failure simulation:

- `juju status mysql` shows the application remains `active`.

- Primary responsibilities move to a different surviving unit.

- Remaining units stay healthy on surviving machines.

## Summary

You have used Juju and Multipass to validate Charmed MySQL high availability with unit spreading and primary failover. This is a practical way to test model-driven operations where Juju placement and charm logic manage recovery behavior.

## Teardown

When you are finished testing, remove the resources created by this tutorial.

For a fast rerun, use this order:

```bash

# Re-enable any zone you disabled during the failover test so Multipass
# state is clean before tearing down.
multipass enable-zones --all

# Remove the workload model and its storage first.
juju destroy-model az-mysql-lab --no-prompt --destroy-storage --force

# Stop and remove the controller. `--timeout 0s` skips the grace wait.
juju kill-controller mp-controller --no-prompt --timeout 0s

# Remove local client-side cloud metadata so reruns start clean.
juju remove-cloud mp-manual --client

# Remove the Multipass instances.
multipass delete juju-1 juju-2 juju-3 juju-controller
multipass purge

```

If the controller is already unreachable and `kill-controller` still hangs, delete/purge the Multipass instances first, then clean local Juju metadata:

```bash
juju unregister mp-controller --no-prompt
juju remove-cloud mp-manual --client --force
```

``````{important}

If you installed Juju only for this tutorial, you can also uninstall it from your host.



`````{tab-set}



````{tab-item} Linux
:sync: linux



```bash

sudo snap remove juju

```



````



````{tab-item} macOS
:sync: macos



```bash

sudo rm /usr/local/bin/juju

```



````



`````

``````
