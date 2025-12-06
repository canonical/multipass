(explanation-blueprint)=
# Blueprint (removed)
```{Warning}
Blueprints have been removed. You can achieve similar effects with cloud-init and other launch options. Find out more at: [Launch customized instances with Multipass and cloud-init](/how-to-guides/manage-instances/launch-customized-instances-with-multipass-and-cloud-init)
```
> See also: [How to use a blueprint](/how-to-guides/manage-instances/use-a-blueprint)

In Multipass, a **blueprint** is a recipe to create a customised Multipass [instance](/explanation/instance).

Blueprints consist of a base image, cloud-init initialisation, and a set of parameters describing the instance itself, e.g. minimum memory, CPUs or disk.

Blueprints are defined from a YAML file with the following schema:

```{code-block} text
# v1/<name>.yaml

description: <string>      # * a short description of the blueprint ("tagline")
version: <string>          # * a version string

runs-on:                   # a list of architectures this blueprint can run on
- arm64                    #   see https://doc.qt.io/qt-5/qsysinfo.html#currentCpuArchitecture
- x86_64                   #   for a list of valid values

instances:
  <name>:                  # * equal to the blueprint name
    image: <base image>    # a valid image alias, see `multipass find` for available values
    limits:
      min-cpu: <int>       # the minimum number of CPUs this blueprint can work with
      min-mem: <string>    # the minimum amount of memory (can use G/K/M/B suffixes)
      min-disk: <string>   # and the minimum disk size (as above)
    timeout: <int>         # maximum time for the instance to launch, and separately for cloud-init to complete
    cloud-init:
      vendor-data: |       # cloud-init vendor data
        <string>

health-check: |            # a health-check shell script ran by integration tests
  <string>

```

Blueprints currently integrated into Multipass can be found with the [`find`](/reference/command-line-interface/find) command.

For more information on creating a blueprint for inclusion into Multipass, please refer to the [GitHub project](https://github.com/canonical/multipass-blueprints).
