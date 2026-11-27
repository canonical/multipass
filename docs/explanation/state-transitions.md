(explanation-state-transitions)=
# State transitions

> See also: {ref}`Instance states <reference-instance-states>`, {ref}`Instance <explanation-instance>`, {ref}`Availability zone <explanation-availability-zone>`, {ref}`Snapshot <explanation-snapshot>`

Every Multipass instance is always in a single state: such as *Running*, *Stopped*, or *Suspended*, as reported by [`multipass list`](reference-command-line-interface-list) and [`multipass info`](reference-command-line-interface-info). Its state reflects its current lifecycle status and determines which commands it accepts, and a **state transition** occurs whenever a command, an availability zone change, or a completed operation moves the instance from one state to another. This page explains how and why these transitions happen; for definitions of each state, see the {ref}`Instance states <reference-instance-states>` reference.

## How instance states are grouped

Instance states fall into three broad groups:

- **Steady states** (*Running*, *Stopped*, *Suspended*, and *Unavailable*): An instance stays in one of these states until you take an action or its availability zone changes.
- **In-progress states** (*Starting*, *Restarting*, *Suspending*, and *Delayed shutdown*): These show that a change is underway. Once the change finishes, the instance moves to a steady state on its own.
- **Unknown**: Multipass started the instance, but couldn't reach it afterward.

## Overview of state transitions

The diagram below maps out every state and how an instance moves between them. Arrows with labels show transitions triggered by a command or event, while unlabeled arrows happen automatically once an in-progress step finishes. The green area groups the states that a forced stop (`multipass stop --force`) can turn off right away, and the blue area groups every state that becomes *Unavailable* when you disable an availability zone (`multipass disable-zones`).

```{mermaid}
flowchart TB
    subgraph Available[" "]
        subgraph NonStopped["Non-stopped available states"]
            Suspended((Suspended))
            Suspending((Suspending))
            Restarting((Restarting))
            Starting((Starting))
            Running((Running))
            Delayed((Delayed shutdown))
            Unknown((Unknown))
        end
        Stopped[Stopped]
        AvailableLabel["Available states"]
    end
    Unavailable[Unavailable]

    Suspended -->|start| Starting
    Starting --> Running
    Running -->|restart| Restarting
    Restarting --> Running
    Running -->|suspend| Suspending
    Suspending --> Suspended
    Running -->|"stop --time N"| Delayed
    Starting -->|No connection| Unknown
    Restarting -->|No connection| Unknown

    Running -->|stop| Stopped
    Unknown -->|stop| Stopped
    Delayed --> Stopped
    Delayed -->|"stop --cancel"| Running
    Stopped -->|start| Starting
    NonStopped -->|"stop --force"| Stopped

    Available -->|disable-zones| Unavailable
    Unavailable -->|enable-zones| Starting
    Unavailable -->|enable-zones| Stopped
    Stopped ~~~ AvailableLabel
    AvailableLabel ~~~ Unavailable

    style Available fill:#4a90d9,fill-opacity:0.15,stroke:#4a90d9
    style NonStopped fill:#4caf50,fill-opacity:0.15,stroke:#4caf50
    style AvailableLabel fill:none,stroke:none
```

The sections below walk through each kind of transition and the reasoning behind it.

## Starting an instance

Starting an instance with [`multipass start`](reference-command-line-interface-start) brings it back into use, though what happens behind the scenes depends on how you left it:

- From *Stopped*, the instance starts up from scratch, just like turning on a computer that was shut down.
- From *Suspended*, it picks up right where you left off with the same open programs and files, much like waking a laptop from sleep.

In both cases, the instance stays in *Starting* until Multipass confirms it can connect, and then switches to *Running*. If Multipass can't connect in time, the state becomes *Unknown*.

Starting an instance that's in *Delayed shutdown* cancels the countdown so it stays *Running*. On the other hand, Multipass won't start an instance that's *Suspending*, *Unavailable*, or *Unknown*. If an instance is *Unknown*, you need to stop it first before starting it again.

```{mermaid}
flowchart LR
    Stopped[Stopped] -->|start| Starting((Starting))
    Suspended((Suspended)) -->|start| Starting
    Starting -->|Connected| Running((Running))
    Starting -->|No connection| Unknown((Unknown))
```

## Restarting an instance

Restarting with [`multipass restart`](reference-command-line-interface-restart) shuts an instance down and starts it up again in one step. While this happens, the instance moves from *Running* to *Restarting*, and returns to *Running* once Multipass can connect again. A restart is useful after changes that only take effect on a fresh start, such as certain system updates.

You can also run a restart on instances that aren't currently running, and Multipass adapts to where the instance already is:

- A *Stopped* instance simply starts up, just like running `multipass start`.
- A *Suspended* instance wakes up first (*Starting*, then *Running*) before rebooting (*Restarting*), because a paused instance needs to wake up before it can close everything cleanly and start fresh.
- An instance in *Delayed shutdown* cancels its pending shutdown and restarts right away.

Multipass won't restart an instance that's already in the middle of a transition (such as *Starting* or *Suspending*) or in *Unknown*. In those cases, Multipass suggests forcing it off with `multipass stop --force` first.

```{mermaid}
flowchart LR
    Stopped[Stopped] --> StartingFromStopped((Starting))
    RunningBefore((Running)) -->|restart| Restarting((Restarting))
    Suspended((Suspended)) --> StartingFromSuspended((Starting))
    StartingFromSuspended --> RunningAfterResume((Running))
    RunningAfterResume --> Restarting

    StartingFromStopped -->|Connected| Running((Running))
    Restarting -->|Connected| Running
    StartingFromStopped --->|No connection| Unknown((Unknown))
    StartingFromSuspended -->|No connection| Unknown
    Restarting -->|No connection| Unknown
```

## Suspending an instance

Suspending an instance with [`multipass suspend`](reference-command-line-interface-suspend) is like putting a laptop to sleep or hibernating it. Multipass saves everything the instance currently has open in memory, then turns it off. The instance shows as *Suspending* while its memory is being saved, and *Suspended* once it's safely paused.

### Choosing between suspending and stopping

Both suspending and stopping turn an instance off so it's no longer running, but they serve different needs. Suspending preserves your open programs and work in progress, so starting the instance later brings you right back to where you stopped. Stopping closes all programs and shuts the instance down completely, so the next start gives you a clean slate.

Suspend is usually the better choice when you're taking a break and plan to continue the same work later. Stop is the better choice when you want a fresh start or won't need the instance for a while.

```{mermaid}
flowchart LR
    Running((Running)) -->|suspend| Suspending((Suspending))
    Suspending --> Suspended((Suspended))
```

## Stopping an instance

Stopping an instance with [`multipass stop`](reference-command-line-interface-stop) turns it off, and Multipass gives you three ways to do it depending on how gentle or urgent the shutdown needs to be:

- **Normal stop:** Running `multipass stop` works like shutting down a computer from its menu. Programs inside the instance get a chance to finish up and close properly before the instance moves from *Running* (or *Unknown*) to *Stopped*.
- **Delayed stop:** Adding `--time N` schedules the shutdown for *N* minutes later. During the countdown, the instance enters *Delayed shutdown*, keeps working normally, and warns anyone signed in so they have time to save their work. If you change your mind before the time runs out, running `multipass stop --cancel` stops the countdown and returns the instance to *Running*.
- **Forced stop:** Adding `--force` is like pulling the power cord. Multipass switches the instance off immediately without waiting for programs to close, which means any unsaved work inside is lost. Because it doesn't wait for the instance to respond, a forced stop works from any active or stuck state (the green area in the diagram), including in-progress states and *Unknown* where a normal stop can't help. If you force-stop a *Suspended* instance, its saved session is discarded and it becomes *Stopped*, so the next start will begin fresh instead of resuming.

```{mermaid}
flowchart TB
    Running((Running)) -->|"stop --time N"| Delayed((Delayed shutdown))
    Running -->|stop| Stopped[Stopped]
    Delayed --> Stopped
    Delayed -->|"stop --cancel"| Running
    NonStopped["Non-stopped available states"] -->|"stop --force"| Stopped

    style NonStopped fill:#4caf50,fill-opacity:0.15,stroke:#4caf50
```

## Simulating outages with availability zones

Every instance lives in an {ref}`availability zone <explanation-availability-zone>`, which lets you group instances and test how your setup behaves if part of it suddenly goes offline.

### Disabling a zone

Running [`multipass disable-zones`](reference-command-line-interface-disable-zones) simulates a sudden power or network outage for one or more zones. Every instance in a disabled zone is switched off immediately—no matter what state it was in—and becomes *Unavailable*. While the zone stays disabled, Multipass won't let you start, restart, or make changes to those instances.

```{mermaid}
flowchart LR
    Available["Available states"] -->|disable-zones| Unavailable[Unavailable]

    style Available fill:#4a90d9,fill-opacity:0.15,stroke:#4a90d9
```

### Re-enabling a zone

Running [`multipass enable-zones`](reference-command-line-interface-enable-zones) ends the simulated outage. To restore things to how they were before the outage, Multipass remembers which instances were active when the zone was disabled. Instances that were *Running*, *Starting*, or *Restarting* start back up automatically (moving through *Starting* to *Running*, or *Unknown* if Multipass can't connect). All other instances—such as those that were already *Stopped* or *Suspended*—become *Stopped* and stay off until you choose to start them.

```{mermaid}
flowchart LR
    Unavailable[Unavailable] --> Starting((Starting))
    Unavailable --> Stopped[Stopped]
    Starting -->|Connected| Running((Running))
    Starting -->|No connection| Unknown((Unknown))
```

## Snapshots and instance states

A {ref}`snapshot <explanation-snapshot>` saves a copy of an instance at a point in time so you can return to it later. Multipass only takes snapshots of *Stopped* instances, because capturing an instance while nothing is changing ensures a clean, reliable copy.

If you run [`multipass snapshot`](reference-command-line-interface-snapshot) on a *Running* or *Suspended* instance, Multipass asks whether to stop it first. Adding `--restart` answers yes ahead of time and asks Multipass to bring the instance back up afterward:

- A *Running* instance is stopped, captured in a snapshot, and started again.
- A *Suspended* instance can't be shut down cleanly while it's paused, so Multipass wakes it up first (*Starting*, then *Running*), stops it cleanly (*Stopped*), takes the snapshot, and starts it again.
- A *Stopped* instance doesn't need any preparation: Multipass takes the snapshot right away, and with `--restart`, starts the instance afterward.

```{mermaid}
flowchart LR
    RunningBefore((Running)) -->|stop| Stopped[Stopped]
    Suspended((Suspended)) --> StartingFromSuspended((Starting))
    StartingFromSuspended -->|Connected| RunningAfterResume((Running))
    RunningAfterResume -->|stop| Stopped
    Stopped -->|"snapshot taken, then start"| StartingAfterSnapshot((Starting))

    StartingAfterSnapshot -->|Connected| Running((Running))
    StartingFromSuspended -->|No connection| Unknown((Unknown))
    StartingAfterSnapshot -->|No connection| Unknown
```
