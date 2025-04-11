(how-to-guides-manage-instances-launch-customized-virtual-machines-with-multipass-and-cloud-init)=
# Launch customized virtual machines with Multipass and cloud-init

If you want to set up a virtual machine with a specific environment or configuration, you can use the launch command along with a custom cloud-init YAML file and an optional post-launch health check to ensure everything is working correctly.

Below are some common examples of using cloud-init YAML and Multipass commands to create a customized virtual machine setups. In each case, a pre-defined cloud-init YAML file is stored in the repository, but users are also free to create and user their own custom cloud-init configurations.

## 📦 anbox-cloud-appliance
Launch with:
```{code-block} text
multipass launch --name anbox-cloud-appliance --cpus 4 --memory 4G --disk 50G --timeout 900 --cloud-init https://raw.githubusercontent.com/canonical/multipass/refs/heads/cloud-init-yaml/data/cloud-init-yaml/cloud-init-anbox.yaml
```
## ⚙️ charm-dev
Launch with:
```{code-block} text
multipass launch 24.04 --name charm-dev --cpus 2 --memory 4G --disk 50G --timeout 1800 --cloud-init https://raw.githubusercontent.com/canonical/multipass/refs/heads/cloud-init-yaml/data/cloud-init-yaml/cloud-init-charm-dev.yaml
```
Health check:
```{code-block} text
multipass exec charm-dev -- bash -c "
 set -e
 charmcraft version
 mkdir -p hello-world
 cd hello-world
 charmcraft init
 charmcraft pack
"
```
## 🐳 docker
Launch with:
```{code-block} text
multipass launch 24.04 --name docker --cpus 2 --memory 4G --disk 40G --cloud-init https://raw.githubusercontent.com/canonical/multipass/refs/heads/cloud-init-yaml/data/cloud-init-yaml/cloud-init-docker.yaml
```
Health check:
```{code-block} text
multipass exec docker -- bash -c "docker run hello-world"
```
You can also optionally add aliases:

```{code-block} text
multipass prefer docker
multipass alias docker:docker docker
multipass alias docker:docker-compose docker-compose
multipass prefer default
multipass aliases
```
> See also: [`How to use command aliases`](/how-to-guides/manage-instances/use-instance-command-aliases)

## 🎞️ jellyfin
Launch with:
```{code-block} text
multipass launch 22.04 --name jellyfin --cpus 2 --memory 4G --disk 40G --cloud-init https://raw.githubusercontent.com/canonical/multipass/refs/heads/cloud-init-yaml/data/cloud-init-yaml/cloud-init-jellyfin.yaml
```

## ☸️ minikube
Launch with:
```{code-block} text
multipass launch --name minikube --cpus 2 --memory 4G --disk 40G --timeout 1800 --cloud-init https://raw.githubusercontent.com/canonical/multipass/refs/heads/cloud-init-yaml/data/cloud-init-yaml/cloud-init-minikube.yaml
```
Health check:
```{code-block} text
multipass exec minikube -- bash -c "set -e
  minikube status
  kubectl cluster-info"
```

## 🤖 ros2-humble
Launch with:
```{code-block} text
multipass launch 22.04 --name ros2-humble --cpus 2 --memory 4G --disk 40G --timeout 1800 --cloud-init https://raw.githubusercontent.com/canonical/multipass/refs/heads/cloud-init-yaml/data/cloud-init-yaml/cloud-init-ros2-humble.yaml
```
Heath check:
```{code-block} text
multipass exec ros2-humble -- bash -c "
  set -e

  colcon --help
  rosdep --version
  ls /etc/ros/rosdep/sources.list.d/20-default.list
  ls /home/ubuntu/.ros/rosdep/sources.cache

  ls /opt/ros/humble
"
```
## 🤖 ros2-jazzy
Launch with:
```{code-block} text
multipass launch 24.04 --name ros2-jazzy --cpus 2 --memory 4G --disk 40G --timeout 1800 --cloud-init https://raw.githubusercontent.com/canonical/multipass/refs/heads/cloud-init-yaml/data/cloud-init-yaml/cloud-init-ros2-jazzy.yaml
```
Health check:
```{code-block} text
multipass exec ros2-jazzy -- bash -c "
  set -e

  colcon --help
  rosdep --version
  ls /etc/ros/rosdep/sources.list.d/20-default.list
  ls /home/ubuntu/.ros/rosdep/sources.cache

  ls /opt/ros/jazzy
"
```
