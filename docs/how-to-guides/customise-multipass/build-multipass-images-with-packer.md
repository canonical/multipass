(how-to-guides-customise-multipass-build-multipass-images-with-packer)=
# Build Multipass images with Packer

```{note}
Custom images are only supported on Linux.
```

[Packer](https://developer.hashicorp.com/packer) is a utility that lets you (re)build images to run in a variety of environments. Multipass can run those images too, provided some requirements are met (namely, the image has to boot on the hypervisor in use, and [cloud-init](https://cloudinit.readthedocs.io/en/latest/) needs to be available).

## Setting up the build directory

The easiest way is to start from an existing [Ubuntu Cloud Image](https://cloud-images.ubuntu.com/), and the base project setup follows (you can click on the filenames to see their contents, `meta-data` is empty on purpose):

```{code-block} text
├── cloud-data
│   ├── meta-data
│   └── <a href="https://paste.ubuntu.com/p/6vbtNXttqZ/">user-data</a>
└── <a href="https://pastebin.ubuntu.com/p/nJsGtWk2N3/">template.json</a>

1 directory, 3 files
```

To specify a different image or image type, modify the `iso_checksum` and `iso_url` fields within `template.json`

## Building the image

You will need to install QEMU and Packer (e.g. `sudo apt install qemu packer`), and from there you can run the following commands:

```{code-block} text
packer build template.json
multipass launch file://$PWD/output-qemu/packer-qemu --disk 5G
```

Now, shell into the new instance that was created, for example:

```{code-block} text
multipass shell tolerant-hammerhead
```

## Customising the image

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

Go to [Packer's documentation](https://developer.hashicorp.com/packer/docs) to learn about the QEMU builder, other provisioners and their configurations as well as everything else that might come in handy. Alternatively, you could extend `user-data` with other `cloud-init` [modules](https://cloudinit.readthedocs.io/en/latest/reference/modules.html) to provision your image.

Join the discussion on the [Multipass forum](https://discourse.ubuntu.com/c/project/multipass/21/) and let us know about your images!
