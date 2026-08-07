(reference-instance-states)=
# Instance states

> See also: [Command-line interface](/reference/command-line-interface/index), [Availability zone](/explanation/availability-zone)

Instances in Multipass can be in a number of different states:

|State | Description|
|--- | ---|
| **Running** | The instance is currently running and is ready to be used. |
| **Stopped** | The instance has been intentionally stopped and is not currently consuming resources. It can be started when needed. |
| **Deleted** | The instance has been marked for deletion. The instance can either be recovered or purged. |
| **Starting**| The instance is in the process of being started up and initialised. It will transition to the *Running* state once fully started. |
| **Restarting** | The instance is undergoing a restart. This involves stopping the instance and then starting it again. |
| **Delayed shutdown** | The instance has been sent a shutdown signal and will be stopped after a specified delay. This allows for any ongoing processes to be completed before shutdown. |
| **Suspending** | This instance is in the process of being suspended. The instance's state and memory will be saved, allowing it to be resumed from where it left off. |
| **Suspended** | The instance has been suspended, meaning its state and memory have been saved. It can be resumed from this state to continue its operation. |
| **Unavailable** | The instance's [availability zone](/explanation/availability-zone) has been disabled with [`disable-zones`](/reference/command-line-interface/disable-zones), so the instance has been forcefully switched off. It will start again automatically once its zone is re-enabled with [`enable-zones`](/reference/command-line-interface/enable-zones). |
| **Unknown** | The state of the instance cannot be determined or retrieved. This might occur due to unexpected errors or issues with Multipass. |

<!--
- `Running`: The instance is currently running and is ready to be used.
- `Stopped`: The instance has been intentionally stopped and is not currently consuming resources. It can be started when needed.
- `Deleted`: The instance has been marked for deletion. The instance can either be recovered or purged.
- `Starting`: The instance is in the process of being started up and initialised. It will transition to the `Running` state once fully started.
- `Restarting`: The instance is undergoing a restart. This involves stopping the instance and then starting it again.
- `Delayed Shutdown`: The instance has been sent a shutdown signal and will be stopped after a specified delay. This allows for any ongoing processes to be completed before shutdown.
- `Suspending`: This instance is in the process of being suspended. The instance's state and memory will be saved, allowing it to be resumed from where it left off.
- `Suspended`: The instance has been suspended, meaning its state and memory have been saved. It can be resumed from this state to continue its operation.
- `Unavailable`: The instance's availability zone has been disabled, so the instance has been forcefully switched off. It will start again automatically once its zone is re-enabled.
- `Unknown`: The state of the instance cannot be determined or retrieved. This might occur due to unexpected errors or issues with Multipass.
-->

These instance states reflect the various stages an instance can be in while using Multipass. Instances in different states can accept different commands. See [Command-line interface](/reference/command-line-interface/index) for more information on which commands can be used and when.
