(tutorial-availability-zones)=
# Multipass availability zones with a load-balanced web service

In this tutorial, we will use Multipass availability zones to build a simple, highly available web service. We will deploy three Nginx web servers, one in each availability zone, and a fourth instance acting as a load balancer to distribute traffic between them.

In the real world, availability zones are clusters of data centers in a particular region. Multipass provides a local simulation of availability zones for development purposes.

To complete this tutorial, you need Multipass 1.17 or later installed on your host.

## Check the available zones

Multipass ships with a fixed set of availability zones. List them, along with their status, before you start:

```bash
multipass zones
```

Sample output:

```text
Name    State       Subnet
zone1   Available   192.168.252.0/24
zone2   Available   192.168.253.0/24
zone3   Available   192.168.254.0/24
```

```{note}
Multipass assigns each zone its own subnet, simulating the network separation between real-world availability zones.
```

We will spread our web servers across `zone1`, `zone2` and `zone3`.

## Launch the web servers

First, let's launch one web server in each availability zone. We will customize their landing pages so we can easily see which zone is responding.

### Launch and configure the first server (zone1)

```bash
multipass launch --name web-a --zone zone1
multipass exec web-a -- sudo apt-get update
multipass exec web-a -- sudo apt-get install -y nginx
```

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
multipass exec web-a -- bash -c 'echo "<h1>Welcome to web-a in zone1</h1>" | sudo tee /var/www/html/index.html'
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
multipass exec web-a -- bash -c "echo '<h1>Welcome to web-a in zone1</h1>' | sudo tee /var/www/html/index.html"
```

````

`````

### Launch and configure the second server (zone2)

```bash
multipass launch --name web-b --zone zone2
multipass exec web-b -- sudo apt-get update
multipass exec web-b -- sudo apt-get install -y nginx
```

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
multipass exec web-b -- bash -c 'echo "<h1>Welcome to web-b in zone2</h1>" | sudo tee /var/www/html/index.html'
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
multipass exec web-b -- bash -c "echo '<h1>Welcome to web-b in zone2</h1>' | sudo tee /var/www/html/index.html"
```

````

`````

### Launch and configure the third server (zone3)

```bash
multipass launch --name web-c --zone zone3
multipass exec web-c -- sudo apt-get update
multipass exec web-c -- sudo apt-get install -y nginx
```

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
multipass exec web-c -- bash -c 'echo "<h1>Welcome to web-c in zone3</h1>" | sudo tee /var/www/html/index.html'
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
multipass exec web-c -- bash -c "echo '<h1>Welcome to web-c in zone3</h1>' | sudo tee /var/www/html/index.html"
```

````

`````

## Launch the load balancer

Now we will launch a fourth instance to act as a load balancer. We will place it explicitly in `zone3` so we know exactly where it lives, and use HAProxy, a popular open-source load balancer, to distribute traffic across all three zones.

```bash
multipass launch --name load-balancer --zone zone3
multipass exec load-balancer -- sudo apt-get update
multipass exec load-balancer -- sudo apt-get install -y haproxy
```

### Configure HAProxy

The load balancer needs the IP address of each web server so it can send incoming requests to them. Get these addresses:

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
WEB_A_IP=$(multipass info web-a --format csv | awk -F, 'NR>1 {print $5}')
WEB_B_IP=$(multipass info web-b --format csv | awk -F, 'NR>1 {print $5}')
WEB_C_IP=$(multipass info web-c --format csv | awk -F, 'NR>1 {print $5}')
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
$WEB_A_IP = (multipass info web-a --format csv | ConvertFrom-Csv).Ipv4
$WEB_B_IP = (multipass info web-b --format csv | ConvertFrom-Csv).Ipv4
$WEB_C_IP = (multipass info web-c --format csv | ConvertFrom-Csv).Ipv4
```

````

`````

Create a configuration file locally and transfer it to the load balancer:

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
cat << EOF > haproxy.cfg

frontend http_front
    bind *:80
    default_backend http_back

backend http_back
    balance roundrobin
    server web-a $WEB_A_IP:80 check
    server web-b $WEB_B_IP:80 check
    server web-c $WEB_C_IP:80 check
EOF

multipass transfer haproxy.cfg load-balancer:
multipass exec load-balancer -- sudo mv /home/ubuntu/haproxy.cfg /etc/haproxy/haproxy.cfg
multipass exec load-balancer -- sudo systemctl restart haproxy
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
@"
frontend http_front
    bind *:80
    default_backend http_back

backend http_back
    balance roundrobin
    server web-a ${WEB_A_IP}:80 check
    server web-b ${WEB_B_IP}:80 check
    server web-c ${WEB_C_IP}:80 check
"@ | Set-Content -Encoding ascii haproxy.cfg

multipass transfer haproxy.cfg load-balancer:
multipass exec load-balancer -- sudo mv /home/ubuntu/haproxy.cfg /etc/haproxy/haproxy.cfg
multipass exec load-balancer -- sudo systemctl restart haproxy
```

````

`````

## Test the high availability

Find the IP address of your load balancer:

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
LB_IP=$(multipass info load-balancer --format csv | awk -F, 'NR>1 {print $5}')
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
$LB_IP = (multipass info load-balancer --format csv | ConvertFrom-Csv).Ipv4
```

````

`````

From now on, send requests only to the load balancer. It decides which healthy backend server responds. Query it once:

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
curl http://$LB_IP
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
curl.exe "http://$LB_IP"
```

````

`````

*Expected output:*

```text
<h1>Welcome to web-a in zone1</h1>
```

Run the command again:

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
curl http://$LB_IP
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
curl.exe "http://$LB_IP"
```

````

`````

This time, the response comes from the next web server in another availability zone:

```text
<h1>Welcome to web-b in zone2</h1>
```

### Simulate a zone failure

To simulate an outage of an entire availability zone, disable `zone1`. Multipass forcefully switches off every instance in that zone and keeps them off until the zone is re-enabled, mirroring a real cloud provider losing a zone:

```bash
multipass disable-zones zone1
```

After a few moments, query the same load balancer address twice. HAProxy detects that `web-a` is unavailable and sends the requests to the surviving web servers:

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
curl http://$LB_IP
curl http://$LB_IP
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
curl.exe "http://$LB_IP"
curl.exe "http://$LB_IP"
```

````

`````

*Expected output (the order may vary):*

```text
<h1>Welcome to web-b in zone2</h1>
<h1>Welcome to web-c in zone3</h1>
```

Notice that none of the responses comes from `web-a` in `zone1`.

### Take down a second zone

Now disable `zone2` as well, leaving only `zone3` healthy:

```bash
multipass disable-zones zone2
```

Query the load balancer again. With two zones down, every request can only come from `web-c` in `zone3`:

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
curl http://$LB_IP
curl http://$LB_IP
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
curl.exe "http://$LB_IP"
```

````

`````

*Expected output:*

```text
<h1>Welcome to web-c in zone3</h1>
```

## Tear down the environment

### Restore the zones

Bring both zones back online. Instances that were running when the zones were disabled are started again automatically:

```bash
multipass enable-zones zone1 zone2
```

After a few moments, `web-a` and `web-b` rejoin the rotation and the load balancer serves all three zones once more.

Let's now delete the instances, free their resources on our host machine, and remove the local HAProxy configuration file:

`````{tab-set}

````{tab-item} Linux/macOS
:sync: Linux/macOS

```bash
multipass delete --purge web-a web-b web-c load-balancer
rm haproxy.cfg
```

````

````{tab-item} Windows PowerShell
:sync: Windows PowerShell

```powershell
multipass delete --purge web-a web-b web-c load-balancer
Remove-Item haproxy.cfg
```

````

`````

## Summary

You have built a highly available web service that spans all three availability zones. Even when an entire zone goes offline, your users can still access the application through the healthy zones, demonstrating the power of infrastructure redundancy with Multipass.
