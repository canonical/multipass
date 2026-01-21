(how-to-guides-customise-multipass-authenticate-users-with-the-multipass-service)=
# Authenticate users with the Multipass service

> See also: [`authenticate`](reference-command-line-interface-authenticate), [local.passphrase](reference-settings-local-passphrase), [Service](explanation-service)

Multipass requires users to be authenticated with the service before allowing commands to complete. The installing user is automatically authenticated.

## Setting the passphrase

The administrator needs to set a passphrase for users to authenticate with the Multipass service. The user setting the passphrase will need to already be authenticated.

There are two ways to proceed:

* Set the passphrase with an echoless interactive entry, where the passphrase is hidden from view:

   ```{code-block} text
   multipass set local.passphrase
   ```

   The system will then prompt you to enter a passphrase:

   ```{code-block} text
   Please enter passphrase:
   Please re-enter passphrase:
   ```

* Set the passphrase in the command line, where the passphrase is visible:

   ```{code-block} text
   multipass set local.passphrase=foo
   ```

## Authenticating the user

A user that is not authorized to connect to the Multipass service will fail when running `multipass` commands. An error will be displayed when this happens.

For example, if you try running the `multipass list` command:

```{code-block} text
list failed: The user is not authenticated with the Multipass service.

Please authenticate before proceeding (e.g. via 'multipass authenticate'). Note that you first need an authenticated user to set and provide you with a trusted passphrase (e.g. via 'multipass set local.passphrase').
```

At this time, the user will need to provide the previously set passphrase. This can be accomplished in two ways:

* Authenticate with an echoless interactive entry, where the passphrase is hidden from view:

    ```{code-block} text
    multipass authenticate
    ```

    The system will prompt you to enter the passphrase:

     ```{code-block} text
    Please enter passphrase:
    ```

* Authenticate in the command line, where the passphrase is visible:

   ```{code-block} text
   multipass authenticate foo
   ```

## Troubleshooting

Here you can find solutions and workarounds for common issues that may arise.

### The user cannot be authorized and the passphrase cannot be set

It is possible that another user that is privileged to connect to the Multipass socket will
connect first and make it seemingly impossible to set the `local.passphrase` and also `authorize`
the user with the service. This usually occurs when Multipass is installed as `root/admin` but
the user is run as another user, or vice versa.

If this is the case, you will see something like the following when you run:

* `multipass list`

  ```{code-block} text
  list failed: The user is not authenticated with the Multipass service.

  Please authenticate before proceeding (e.g. via 'multipass authenticate'). Note that you first need an authenticated user to set and provide you with a trusted passphrase (e.g. via 'multipass set local.passphrase').
  ```

* `multipass authenticate`

  ```{code-block} text
  Please enter passphrase:
  authenticate failed: No passphrase is set.

  Please ask an authenticated user to set one and provide it to you. They can achieve so with 'multipass set local.passphrase'. Note that only the user who installs Multipass is automatically authenticated.
  ```

* `multipass set local.passphrase`

  ```{code-block} text
  Please enter passphrase:
  Please re-enter passphrase:
  set failed: The user is not authenticated with the Multipass service.

  Please authenticate before proceeding (e.g. via 'multipass authenticate'). Note that you first need an authenticated user to set and provide you with a trusted passphrase (e.g. via 'multipass set local.passphrase').
  ```

This may not even work when using `sudo`.

The following workaround should help get out of this situation:

```bash
cat ~/snap/multipass/current/data/multipass-client-certificate/multipass_cert.pem | sudo tee -a /var/snap/multipass/common/data/multipassd/authenticated-certs/multipass_client_certs.pem > /dev/null

snap restart multipass
```

You may need `sudo` with this last command: `sudo snap restart multipass`.

At this point, your user should be authenticated with the Multipass service.
