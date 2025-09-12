(tutorial-az-mysql)=
# High Availability MySQL with Multipass Availability Zones

In this tutorial, you will learn how to configure and use cloud-style high availability zones using Multipass. You will set up isolated virtual environments that behave like real-world availability zones, allowing you to experiment with fault-tolerant architectures entirely on your local machine.

> See more: [zones explanation](explanation-availability-zones)

## What you’ll need

- Hardware requirements: 8 CPUs, 2 GB RAM, and 10 GB disk (for two VMs, each requiring 4 CPUs, 1 GB RAM, 5 GB disk).
- Multipass 1.17+ installed

## What you’ll do

- Set up two identical VMs across two separate zones
- Set up up primary and secondary MySQL databases on the VMs
- Orchestrate master-replica replication across the two zones
- Configure automatic failover
- Test and validate the recovery
  
By the end of this tutorial, you will have a working high availability MySQL environment capable of promoting replicas, reintegrating failed nodes, and maintaining service continuity entirely within a local setup.

``` {important}
The process has been validated using Multipass 1.17 and MySQL 8.0.
```

## 1. Set up the VMs

Begin by launching two virtual machines, `mysql-primary` and `mysql-standby`, and distributing them across different zones (in this case zone1 and zone2) to simulate distinct availability zones

```bash
multipass launch --name mysql-primary --zone zone1 
multipass launch --name mysql-standby --zone zone2
```

After launching, install MySQL Server on both VMs:

```bash
for vm in mysql-primary mysql-standby; do
  multipass exec $vm -- sudo apt update
  multipass exec $vm -- sudo apt install -y mysql-server
done
```

This installation process ensures necessary packages are in place.

## 2. Configure the primary MySQL server

Access the primary VM (mysql-primary) to begin its configuration.

```bash
multipass shell mysql-primary
```

Open the MySQL configuration file

```bash
sudo nano /etc/mysql/mysql.conf.d/mysqld.cnf
```

You should check that the following settings are present, then save the file after the edit.

```ini
server-id=1
log_bin=mysql-bin
binlog_do_db=testdb
```

- `server-id=1` setting gives the MySQL server a unique ID in the replication topology. Without this, replication won’t work because MySQL cannot tell servers apart.
  
  ```{important}
  Every server in the replication setup (primary and replicas) must have a distinct server-id.
  ```

- `binlog_do_db=testdb` setting specifies which databases to include in the binary log. This is important for replication, as replicas need to know which databases to replicate.
- `log_bin=mysql-bin` setting enables the binary log on the primary server and the log records every change to databases (INSERT, UPDATE, DELETE, etc.).
  Replicas read from this log to replay the changes and stay synchronized with the primary.

Restart MySQL after applying these changes.

```bash
sudo systemctl restart mysql
```

Create a replication user with the necessary permissions.

```bash
sudo mysql -u root <<EOF
CREATE USER 'replica'@'%' IDENTIFIED BY 'replica_pass';
GRANT REPLICATION SLAVE ON *.* TO 'replica'@'%';
FLUSH PRIVILEGES;
EOF
```

Create an initial test database and a table for replication verification

```bash
sudo mysql -u root -e "CREATE DATABASE testdb; USE testdb; CREATE TABLE t1 (id INT); INSERT INTO t1 VALUES (1);"
```

## 3. Configure the standby VM as a replica

In a new terminal, access the standby VM (mysql-standby) for its configuration:

```bash
multipass shell mysql-standby
```

Edit the MySQL configuration file `/etc/mysql/mysql.conf.d/mysqld.cnf` to include these settings:

```bash
sudo nano /etc/mysql/mysql.conf.d/mysqld.cnf
```

Add or modify the following lines:

```ini
server-id=2
relay-log=relay-log
```

Restart MySQL after configuration to apply the changes:

```bash
sudo systemctl restart mysql
```

Go to the first terminal running the shell instance of `mysql-primary`.

From the primary VM, obtain the master status, noting the `File` and `Position` values for replication setup:

```bash
sudo mysql -u root -e "SHOW MASTER STATUS\G"
```

The output will be similar to this:

``` {terminal}
*************************** 1. row ***************************
             File: mysql-bin.000001
         Position: 1524
     Binlog_Do_DB: testdb
 Binlog_Ignore_DB: 
Executed_Gtid_Set: 
```

Take note of:

- File: e.g. `mysql-bin.000001`
- Position: e.g. `1524`

Dump the primary database to create a baseline for the replica.

```bash
sudo mysqldump -u root --all-databases --master-data=2 --single-transaction --flush-logs --hex-blob > full.sql
```

Check if the file was created successfully.

```
> ls

full.sql
```

Since Multipass does not support direct transfers between instances, we will first transfer the `full.sql` file to our local machine.

Open a third terminal and run this command:

```bash
multipass transfer mysql-primary:full.sql .
```

We need to check if the file exists on our local machine:

```
> ls

full.sql
```

Copy the dumped database file to the standby VM.

```bash
multipass transfer full.sql  mysql-standby:
```

On the standby VM (second terminal), you can first check if the file was successfully imported.

```
> ls

full.sql
```

In the stand-by VM, import the dumped database.

```bash
sudo mysql < full.sql
```

Extract the replication log file and position from the `full.sql` file.

```bash
grep "^-- CHANGE" full.sql
```

The output will be similar to this.  (Take note of the `MASTER_LOG_FILE` and `MASTER_LOG_POS` values)

```{terminal}
-- CHANGE MASTER TO MASTER_LOG_FILE='mysql-bin.000002', MASTER_LOG_POS=157;
```

Finally, configure the standby as a replica by providing the primary's IP, replication user credentials, and the extracted master log file and position.

```bash
sudo mysql -u root <<EOF
STOP SLAVE;
RESET SLAVE ALL;
CHANGE MASTER TO
  MASTER_HOST='192.168.2.12',
  MASTER_USER='replica',
  MASTER_PASSWORD='replica_pass',
  MASTER_LOG_FILE='mysql-bin.000002',
  MASTER_LOG_POS=157;
START SLAVE;
EOF
```

NOTE: The `PRIMARY_IP` value can either be obtained from the Multipass GUI or by running `multipass list`. In this case, it is `192.168.2.12`.

```bash
Name                    State             IPv4             Image
mysql-primary           Running           192.168.2.12     Ubuntu 24.04 LTS
mysql-standby           Running           192.168.2.13     Ubuntu 24.04 LTS
```

MASTER_USER='replica'and MASTER_PASSWORD='replica_pass’ values were created before in the tutorial.

```bash
sudo mysql -u root <<EOF
STOP SLAVE;
RESET SLAVE ALL;
CHANGE MASTER TO
  MASTER_HOST='192.168.2.12',
  MASTER_USER='replica',
  MASTER_PASSWORD='replica_pass',
  MASTER_LOG_FILE='mysql-bin.000002',
  MASTER_LOG_POS=157;
START SLAVE;
EOF
```

## 4. Verify replication

On the standby, use the following commands to check the slave status:

```bash
sudo mysql -u root -e "SHOW SLAVE STATUS\G"
```

Ensure:

- `Slave_IO_Running`: Yes
- `Slave_SQL_Running`: Yes

You can also verify data by selecting from `testdb.t1`

```bash
mysql -u root -e "SELECT * FROM testdb.t1;"
```

## 5. Implement automatic failover

On mysql-standby, create the failover script `/usr/local/bin/auto_failover.sh`:

```bash
sudo nano /usr/local/bin/auto_failover.sh
```

Paste the following script content, replacing PRIMARY_IP with the actual IP address of mysql-primary:

```bash
#!/bin/bash
PRIMARY_HOST=PRIMARY_IP
MYSQL_USER="root"
MYSQL_PASS=""

mysqladmin -h "$PRIMARY_HOST" -u "$MYSQL_USER" ping > /dev/null 2>&1
if [ $? -eq 0 ]; then
  echo "Primary is alive. No failover needed."
  exit 0
fi

echo "Primary appears down. Initiating failover..."

mysql -u root <<EOF
STOP SLAVE;
RESET SLAVE ALL;
RESET MASTER;
EOF

echo "Failover complete. This node is now primary."
touch /var/lib/mysql/.promoted_to_primary
```

Make the script executable:

```bash
sudo chmod +x /usr/local/bin/auto_failover.sh
```

Edit root's crontab to run this script every minute

```bash
sudo crontab -e
```

Add the following line to the crontab:

```bash
* * * * * /usr/local/bin/auto_failover.sh >> /var/log/mysql-failover.log 2>&1
```

## 6. Automate rejoining of the original primary

On mysql-primary, create the rejoin script `/usr/local/bin/rejoin_cluster.sh`:

```bash
sudo nano /usr/local/bin/rejoin_cluster.sh
```

Paste the following script content, replacing `STANDBY_IP` with the actual IP address of `mysql-standby`:

```bash
#!/bin/bash
NEW_PRIMARY_IP=STANDBY_IP
MYSQL_ROOT="root"
MYSQL_DATA_DIR="/var/lib/mysql"
DUMP_FILE="/tmp/resync.sql"
REPL_USER="replica"
REPL_PASS="replica_pass"

[ -f "$MYSQL_DATA_DIR/.promoted_to_primary" ] && exit 0

mysqldump -h $NEW_PRIMARY_IP -u $MYSQL_ROOT --all-databases --master-data=2 --single-transaction --flush-logs --hex-blob > $DUMP_FILE
[ $? -ne 0 ] && exit 1

systemctl stop mysql
rm -rf $MYSQL_DATA_DIR/*
mysqld --initialize-insecure
systemctl start mysql
mysql -u root < $DUMP_FILE

MASTER_LOG_FILE=$(grep 'MASTER_LOG_FILE' $DUMP_FILE | sed -E "s/.*MASTER_LOG_FILE='([^']+)'.*/\1/")
MASTER_LOG_POS=$(grep 'MASTER_LOG_POS' $DUMP_FILE | sed -E "s/.*MASTER_LOG_POS=([0-9]+).*/\1/")

mysql -u root <<EOF
STOP SLAVE;
RESET SLAVE ALL;
CHANGE MASTER TO
  MASTER_HOST='$NEW_PRIMARY_IP',
  MASTER_USER='$REPL_USER',
  MASTER_PASSWORD='$REPL_PASS',
  MASTER_LOG_FILE='$MASTER_LOG_FILE',
  MASTER_LOG_POS=$MASTER_LOG_POS;
START SLAVE;
EOF

```

```{note}
This creates a permanent role reversal. The original primary doesn't get promoted back - it permanently becomes the slave of what used to be the standby.
This is actually a common pattern in high availability setups because:
- It's simpler to implement
- Avoids potential data conflicts
- The "failed" node has to prove it's healthy by successfully operating as a slave first
- Prevents split-brain scenarios
```

Make the script executable

```bash
sudo chmod +x /usr/local/bin/rejoin_cluster.sh
```

Enable the script to run on boot by creating a systemd service file

```bash
sudo nano /etc/systemd/system/mysql-rejoin.service
```

Add the following to the file:

```ini
[Unit]
Description=Rejoin MySQL cluster if demoted
After=network.target mysql.service

[Service]
ExecStart=/usr/local/bin/rejoin_cluster.sh
Type=oneshot

sudo nano /etc/systemd/system/mysql-rejoin.service
Add the following to the file:
[Unit]
Description=Rejoin MySQL cluster if demoted
After=network.target mysql.service

[Service]
ExecStart=/usr/local/bin/rejoin_cluster.sh
Type=oneshot

[Install]
WantedBy=multi-user.target
```

Enable the systemd service:

```bash
sudo systemctl enable mysql-rejoin.service
```

## 7. Validate High Availability

### Simulate failover

Simulate a failover by disabling zone1, which will shut down `mysql-primary`.

```bash
multipass zones-disable zone1
multipass stop mysql-primary --force # if unavailablity is not supported for backend (functionally identical behavior to zones-disable)
```

Wait 1-2 minutes for the failover to occur. Then, on `mysql-standby`, check the following:

```bash
mysql -u root -e "SHOW SLAVE STATUS\G"  # should be empty
mysql -u root -e "INSERT INTO testdb.t1 VALUES (2);"
```

### Simulate rejoin

Simulate a rejoin by starting mysql-primary again:

```bash
multipass start mysql-primary
```

On mysql-primary, after it boots, check the slave status and data:

```bash
mysql -u root -e "SHOW SLAVE STATUS\G"
mysql -u root -e "SELECT * FROM testdb.t1;"
```

You should see the output below, confirming that the original primary has successfully rejoined as a replica and synchronized the data:

```{terminal}
+------+
| id   |
+------+
|    1 |
|    2 |
+------+
```

You have now set up a MySQL master–replica cluster across Multipass VMs, featuring automatic failover when the primary becomes unavailable and seamless recovery with rejoining when the original primary returns.
