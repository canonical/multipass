(reference-logging-levels)=
# Logging levels

> See also: [Configure Multipass’s default logging level](/how-to-guides/customise-multipass/configure-multipasss-default-logging-level)

In Multipass, a hierarchy of logging levels is used is used to convey severity and improve visibility of important events. Multipass uses the following levels ranked from most severe to least severe for its background daemon and child processes.

## Error

Indicates a failure that prevents the intended operation from being accomplished in its entirety. If there is a corresponding CLI command, it should exit with an error code.

## Warning

Indicates an event or fact that might not correspond to the user's intentions/desires/beliefs, or a problem that is light enough that it does not prevent main goals from being accomplished. If there is a corresponding CLI command, it should exit with a success code.

## Info

Indicates information that may be useful for the user to know, learn, etc.

## Debug

Indicates information that is useful for developers and troubleshooting.

## Trace

Indicates information that may be helpful for debugging, but which would clutter logs unreasonably if enabled by default.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/logging-levels" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

