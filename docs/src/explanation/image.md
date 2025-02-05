(explanation-image)=
# Image

> See also: [`find`](/reference/command-line-interface/find), [`launch`](/reference/command-line-interface/launch)

Multipass uses **images** (short for [disk images](https://en.wikipedia.org/wiki/Disk_image) or [system images](https://en.wikipedia.org/wiki/System_image)) tuned for cloud usage to spin up Ubuntu VMs. 

You can use `multipass find` to view a list of the available images. These images are obtained from different sources, such as:
* Ubuntu Cloud Images: https://cloud-images.ubuntu.com/
* Ubuntu CD Images: https://cdimages.ubuntu.com/

and more. 

You can also launch images from a file or URL, as long as they provide the tools required to deploy a cloud. The key requirements are:
* cloud-init
* SSH

For more information on the system requirements for a particular image, refer to official documentation (for example: [Ubuntu Core system requirements](https://ubuntu.com/core/docs/system-requirements)).

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/image" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

