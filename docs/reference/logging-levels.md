(reference-logging-levels)=
# Logging levels

> See also: [Configure Multipass’s default logging level](/how-to-guides/customise-multipass/configure-multipass-default-logging-level)

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
