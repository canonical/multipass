(explanation-authentication)=
# Authentication

> See also: [How to authenticate users with the Multipass service](how-to-guides-customise-multipass-authenticate-users-with-the-multipass-service)

Before executing any commands, Multipass requires users to authenticate with the service. Multipass employs an authentication process based on x509 certificates signed by elliptic curve (EC) keys, powered by OpenSSL, to authenticate users. When a user connects, Multipass validates the certificate to ensure only verified users can access the service.

`````{tab-set}

````{tab-item} Linux
Linux and macOS hosts currently use a Unix domain socket for client and daemon communication. Upon first use, this socket only allows a client to connect via a user belonging to the group that owns the socket. For example, this group could be `sudo`, `admin`, or `wheel` and the user needs to belong to this group or else permission will be denied when connecting.

After the first client connects with a user belonging to the socket's admin group, the user's OpenSSL certificate will be accepted by the daemon and the socket will then be open for all users to connect. Any other user trying to connect to the Multipass service will need to authenticate with the service using the previously set [`local.passphrase`](/reference/settings/local-passphrase).
````

````{tab-item} macOS
Linux and macOS hosts currently use a Unix domain socket for client and daemon communication. Upon first use, this socket only allows a client to connect via a user belonging to the group that owns the socket. For example, this group could be `sudo`, `admin`, or `wheel` and the user needs to belong to this group or else permission will be denied when connecting.

After the first client connects with a user belonging to the socket's admin group, the user's OpenSSL certificate will be accepted by the daemon and the socket will then be open for all users to connect. Any other user trying to connect to the Multipass service will need to authenticate with the service using the previously set [`local.passphrase`](/reference/settings/local-passphrase).
````

````{tab-item} Windows
The Windows host uses a TCP socket listening on port 50051 for client connections. This socket is open for all to use since there is no concept of file ownership for TCP sockets. This is not very secure in that any Multipass user can connect to the service and issue any commands.

To close this gap, the user will now need to be authenticated with the Multipass service. To ease the burden of having to authenticate, the user who installs the updated version of Multipass will automatically have their clients authenticated with the service. Any other users connecting to the service will have to use authenticate using the previously set [`local.passphrase`](/reference/settings/local-passphrase).
````

`````
