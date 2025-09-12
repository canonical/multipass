(explanation-availability-zones)=
# Understanding Zones

```{important}
It is important to note that availability zones are a simulation-only feature and do not deploy on actual physical zones. Multipass provides the capability to distribute instances across as many as three availability zones. 
```

Multipass availability zones allow you to simulate fault injection at the level of availability zones. In real cloud environments, availability zones are separate datacenters with independent power, cooling, and networking components. By using Multipass availability zones, you can mimic this setup locally to test and improve the fault tolerance of your applications through simulated data redundancy and zone failures.

To conduct an experiment, you can make one or more zones unavailable. This action shuts down all running instances within those zones, enabling you to analyze the resilience of your applications 
