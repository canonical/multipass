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
