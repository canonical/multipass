(explanation-id-mapping)=
# ID mapping

> See also: [`mount`](/reference/command-line-interface/mount), [Mount](/explanation/mount), [How to share data with an instance](/how-to-guides/manage-instances/share-data-with-an-instance)

**ID mapping** refers to the process of aligning user or group IDs between the host system and an instance when mounting directories. This alignment ensures that the files mounted from the host to the instance retain consistent ownership and permission attributes.

Since ID mappings take effect from host to instance, as well as in the opposite direction, they must be defined as a one-to-one relationship, meaning that each user or group ID on the host system should be mapped directly to a single user or group ID within the virtual machine, and vice versa.

For example, the user ID `501` can be mapped to the user ID `1000` in the "foo" instance:

```
multipass mount ~/Documents foo:Documents -u 501:1000
```

On the other hand, it is not possible to map this same user to a second user ID within the instance, as Multipass would be unable to determine which user ID to assign on the instance to a file with a user ID of `501` on the host system. The following command defines an invalid mount, since more than one ID on the host is mapped to the same ID in the instance:

<s>`multipass mount ~/Documents foo:Documents -u 501:1000 -u 502:1000`</s>

Instead, a valid mount that maps two different user IDs could be defined as follows:

```
multipass mount ~/Documents foo:Documents -u 501:1000 -u 502:1001
```

The same logic also applies when trying to map a single user or group ID in an instance to two different IDs on the host.

<!-- Discourse contributors
<small>**Contributors:** @sharder996 , @gzanchi </small>
-->
