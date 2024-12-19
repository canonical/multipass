# Build Multipass images with Packer
[note type="information"]
Custom images are only supported on Linux.
```

[Packer](http://packer.io/) is a utility that lets you (re)build images to run in a variety of environments. Multipass can run those images too, provided some requirements are met (namely, the image has to boot on the hypervisor in use, and [cloud-init](https://cloudinit.readthedocs.io/en/latest/) needs to be available).

## Setting up the build directory

The easiest way is to start from an existing [Ubuntu Cloud Image](http://cloud-images.ubuntu.com/), and the base project setup follows (you can click on the filenames to see their contents, `meta-data` is empty on purpose):

```
├── cloud-data
│   ├── meta-data
│   └── <a href="http://paste.ubuntu.com/p/6vbtNXttqZ/">user-data</a>
└── <a href="https://pastebin.ubuntu.com/p/nJsGtWk2N3/">template.json</a>

1 directory, 3 files
```

To specify a different image or image type, modify the `iso_checksum` and `iso_url` fields within `template.json`

## Building the image

You will need to install QEMU and Packer (e.g. `sudo apt install qemu packer`), and from there you can run the following commands:

```plain
packer build template.json
multipass launch file://$PWD/output-qemu/packer-qemu --disk 5G
```

Now, shell into the new instance that was created, for example:

```plain
multipass shell tolerant-hammerhead
```

## Customizing the image

Now the above works for you, delete the test instance with `multipass delete --purge tolerant-hammerhead` and edit the following section in the `template.json` file:

```json
        {
            "type": "shell",
            "inline": ["echo Your steps go here."]
        },
```

Anything you do here will be reflected in the resulting image. You can install packages, configure services, anything you can do on a running system. You'll need `sudo` (passwordless) for anything requiring admin privileges, or you could add this to the above provisioner to run the whole script privileged:

```json
            "execute_command": "sudo sh -c '{{ .Vars }} {{ .Path }}'",
```

## Next steps

Go to [Packer's documentation](https://developer.hashicorp.com/packer/docs) to learn about the QEMU builder, other provisioners and their configurations as well as everything else that might come in handy. Alternatively, you could extend `user-data` with other `cloud-init` [modules](https://cloudinit.readthedocs.io/en/latest/reference/modules.html#modules) to provision your image.

Join the discussion on the [Multipass forum](https://discourse.ubuntu.com/c/multipass/) and let us know about your images!

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/building-multipass-images-with-packer" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

