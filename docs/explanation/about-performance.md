# About performance
When considering performance with Multipass, there are two aspects that need to be accounted for:

* the [Multipass instance](/explanation/instance) 
* the [host machine](/explanation/host)

There are many factors that can affect the performance of the instance and the host.  To ensure the best performance of both areas, careful consideration should be given. 

## Host system

Here you'll find some considerations and recommendations on allocating host system resources.

### CPUs/cores/threads

When provisioning a Multipass instance, you should take into account the host's CPU speed and the number of cores and threads.  You should also consider the number of instances that will be running simultaneously.  

The number of cores allocated to an instance and the number of running instances will greatly impact processes running on the host machine itself.  As general guidance, you should reserve at least two threads, that will not be allocated to running instances.  

Of course, there may be other factors in different host machines that can greatly affect how well they perform when running Multipass instances.

### Memory usage

The amount of memory allocated to instances can also greatly impact the host system. 

You should not overallocate memory for running Multipass instances, as this can cause the host to appear slower than normal or become unresponsive.  

We recommend that you reserve at least 4GB of memory for the host, but this also depends on the workload of the host itself, so more memory may be needed.

## Multipass instances

Here you'll find some considerations and recommendations on allocating resources to your instances.

### CPUs

The number of CPUs allocated to a Multipass instance has a direct impact on the performance of the instance itself.  Generally, the more CPUs allocated, the more potential for better performance in the instance.  This depends heavily on the workload intended for instance.

### Memory

As with cores, the amount of memory allocated to a Multipass instance will also have a direct on its performance.  This is also dependent on the intended workload of instances. Naturally, more memory intensive workloads provide much larger gains in performance if more memory is allocated to the instance.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/about-performance" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

