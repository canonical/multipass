(explanation-state-transitions)=
# State transitions

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
    Starting -->|No IP found| Unknown
    Restarting -->|No IP found| Unknown

    Running -->|stop| Stopped
    Unknown -->|stop| Stopped
    Delayed --> Stopped
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

## `multipass start`

```{mermaid}
flowchart LR
    Stopped[Stopped] -->|start| Starting((Starting))
    Suspended((Suspended)) -->|start| Starting
    Starting -->|SSH connection| Running((Running))
    Starting -->|No connection| Unknown((Unknown))
```

## `multipass restart`

```{mermaid}
flowchart LR
    Stopped[Stopped] --> StartingFromStopped((Starting))
    RunningBefore((Running)) -->|restart| Restarting((Restarting))
    Suspended((Suspended)) --> StartingFromSuspended((Starting))
    StartingFromSuspended --> RunningAfterResume((Running))
    RunningAfterResume --> Restarting

    StartingFromStopped -->|SSH connection| Running((Running))
    Restarting -->|SSH connection| Running
    StartingFromStopped --->|No connection| Unknown((Unknown))
    StartingFromSuspended -->|No connection| Unknown
    Restarting -->|No connection| Unknown
```

## `multipass suspend`

```{mermaid}
flowchart LR
    Running((Running)) -->|suspend| Suspending((Suspending))
    Suspending --> Suspended((Suspended))
```

## `multipass stop`

```{mermaid}
flowchart TB
    Running((Running)) -->|"stop --time N"| Delayed((Delayed shutdown))
    Running -->|stop| Stopped[Stopped]
    Delayed --> Stopped
    NonStopped["Non-stopped available states"] -->|"stop --force"| Stopped

    style NonStopped fill:#4caf50,fill-opacity:0.15,stroke:#4caf50
```

## `multipass enable-zones`

```{mermaid}
flowchart LR
    Unavailable[Unavailable] --> Starting((Starting))
    Unavailable --> Stopped[Stopped]
    Starting -->|SSH connection| Running((Running))
    Starting -->|No connection| Unknown((Unknown))
```

## `multipass disable-zones`

```{mermaid}
flowchart LR
    Available["Available states"] -->|disable-zones| Unavailable[Unavailable]

    style Available fill:#4a90d9,fill-opacity:0.15,stroke:#4a90d9
```

## `multipass snapshot --restart`

```{mermaid}
flowchart LR
    StoppedBefore[Stopped] --> StartingFromStopped((Starting))
    RunningBefore((Running)) -->|restart| Stopped[Stopped]
    Suspended((Suspended)) --> StartingFromSuspended((Starting))
    StartingFromSuspended --> RunningAfterResume((Running))
    RunningAfterResume --> Stopped
    Stopped --> StartingAfterSnapshot((Starting))

    StartingFromStopped -->|SSH connection| Running((Running))
    StartingAfterSnapshot -->|SSH connection| Running
    StartingFromStopped --->|No connection| Unknown((Unknown))
    StartingFromSuspended -->|No connection| Unknown
    StartingAfterSnapshot -->|No connection| Unknown
```
