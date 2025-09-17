(tutorial-az-mysql)=
# High Availability MySQL with Multipass Availability Zones

In this tutorial, you will learn how to configure and use cloud-style high availability zones using Multipass. You will set up isolated virtual environments that behave like real-world availability zones, allowing you to experiment with fault-tolerant architectures entirely on your local machine.

> See more: [zones explanation](explanation-availability-zones)

## What you'll need

- Hardware requirements: 8 CPUs, 2 GB RAM, and 10 GB disk (for two VMs, each requiring 4 CPUs, 1 GB RAM, 5 GB disk).
- Multipass 1.17+ installed

## What you'll do

- Set up two identical VMs across two separate zones
- Set up primary and secondary MySQL databases on the VMs
- Orchestrate master-replica replication across the two zones with SSL encryption
- Configure automatic failover
- Test and validate the recovery

By the end of this tutorial, you will have a working high availability MySQL environment capable of promoting replicas, reintegrating failed nodes, and maintaining service continuity entirely within a local setup.

``` {important}
The process has been validated using Multipass 1.17 and MySQL 8.0.
```

## 1. Set up the VMs

Begin by launching two virtual machines, `mysql-primary` and `mysql-standby`, and distributing them across different zones (in this case zone1 and zone2) to simulate distinct availability zones:

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

Access the primary VM, `mysql-primary`, to begin its configuration:

```bash
multipass shell mysql-primary
```

First, configure MySQL authentication for replication. Since replication requires network connections, we need to set up password authentication:

```bash
# Connect to MySQL as root using sudo (auth_socket)
sudo mysql << 'EOF'
-- Set a password for root user and enable password authentication
ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY 'SecureRootPass123';
-- Allow root connections from any host (needed for replication)
CREATE USER 'root'@'%' IDENTIFIED BY 'SecureRootPass123';
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
-- Remove anonymous users and test database for security
DROP USER IF EXISTS ''@'localhost';
DROP USER IF EXISTS ''@'%';
DROP DATABASE IF EXISTS test;
FLUSH PRIVILEGES;
EOF
```

**Important**: Replace `SecureRootPass123` with a secure password of your choice throughout this guide.

Now configure MySQL for replication. Open the MySQL configuration file:

```bash
sudo nano /etc/mysql/mysql.conf.d/mysqld.cnf
```

Add these settings to the `[mysqld]` section:

```ini
server-id=1
log_bin=mysql-bin
binlog_do_db=testdb
bind-address=0.0.0.0

# SSL configuration for secure replication
ssl-ca=/etc/mysql/ssl/ca-cert.pem
ssl-cert=/etc/mysql/ssl/server-cert.pem
ssl-key=/etc/mysql/ssl/server-key.pem
```

Restart MySQL after applying these changes:

```bash
sudo systemctl restart mysql
```

Verify MySQL is listening on all interfaces:

```bash
sudo ss -tlnp | grep :3306
```

You should see MySQL listening on `0.0.0.0:3306`. If you see `127.0.0.1:3306`, check that `bind-address=0.0.0.0` is properly set in the configuration file.

Also ensure the firewall allows MySQL connections:

```bash
sudo ufw allow 3306/tcp
```

Now create SSL certificates for secure replication:

```bash
# Create SSL certificates directory
sudo mkdir -p /etc/mysql/ssl

# Get the primary IP address for the certificate
PRIMARY_IP=$(hostname -I | awk '{print $1}')

# Generate CA certificate
sudo openssl genrsa -out /etc/mysql/ssl/ca-key.pem 2048
sudo openssl req -new -x509 -nodes -key /etc/mysql/ssl/ca-key.pem -days 3650 -subj "/CN=MySQL CA" -out /etc/mysql/ssl/ca-cert.pem

# Generate server certificate with IP address as Common Name
sudo openssl req -newkey rsa:2048 -nodes -keyout /etc/mysql/ssl/server-key.pem -days 365 -subj "/CN=$PRIMARY_IP" -out ~/server-req.pem
sudo openssl x509 -req -in ~/server-req.pem -CA /etc/mysql/ssl/ca-cert.pem -CAkey /etc/mysql/ssl/ca-key.pem -CAcreateserial -days 365 -out /etc/mysql/ssl/server-cert.pem

# Generate client certificate for replica
sudo openssl req -newkey rsa:2048 -nodes -keyout /etc/mysql/ssl/client-key.pem -days 365 -subj "/CN=replica" -out ~/client-req.pem
sudo openssl x509 -req -in ~/client-req.pem -CA /etc/mysql/ssl/ca-cert.pem -CAkey /etc/mysql/ssl/ca-key.pem -CAcreateserial -days 365 -out /etc/mysql/ssl/client-cert.pem

# Clean up temporary files and set permissions
sudo rm -f ~/server-req.pem ~/client-req.pem /etc/mysql/ssl/ca-cert.srl
sudo chmod 600 /etc/mysql/ssl/*.pem
sudo chown mysql:mysql /etc/mysql/ssl/*.pem
```

Now create a replication user with SSL requirements:

```bash
# Connect to MySQL with the root password we set earlier
mysql -u root -p << 'EOF'
-- Drop user if it exists, then create it
DROP USER IF EXISTS 'replica'@'%';
CREATE USER 'replica'@'%' IDENTIFIED BY 'replica_pass' REQUIRE SSL;
GRANT REPLICATION SLAVE ON *.* TO 'replica'@'%';
FLUSH PRIVILEGES;
EOF
```

Create an initial test database and a table for replication verification:

```bash
mysql -u root -p -e "CREATE DATABASE testdb; USE testdb; CREATE TABLE t1 (id INT); INSERT INTO t1 VALUES (1);"
```

## 3. Configure the standby VM as a replica

Access the standby VM, `mysql-standby`, for its configuration:

```bash
multipass shell mysql-standby
```

First, configure MySQL authentication on the standby VM to match the primary:

```bash
# Connect to MySQL as root using sudo (auth_socket) and configure password authentication
sudo mysql << 'EOF'
-- Set a password for root user and enable password authentication (same as primary)
ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY 'SecureRootPass123';
-- Allow root connections from any host (needed for potential failover scenarios)
CREATE USER 'root'@'%' IDENTIFIED BY 'SecureRootPass123';
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
-- Remove anonymous users and test database for security
DROP USER IF EXISTS ''@'localhost';
DROP USER IF EXISTS ''@'%';
DROP DATABASE IF EXISTS test;
FLUSH PRIVILEGES;
EOF
```

**Important**: Use the same root password (`SecureRootPass123`) as configured on the primary server.

Edit the MySQL configuration file /etc/mysql/mysql.conf.d/mysqld.cnf to include these settings:

```ini
server-id=2
relay-log=relay-log
bind-address=0.0.0.0

# SSL configuration for secure replication
ssl-ca=/etc/mysql/ssl/ca-cert.pem
ssl-cert=/etc/mysql/ssl/server-cert.pem
ssl-key=/etc/mysql/ssl/server-key.pem
```

Restart MySQL after configuration:

```bash
sudo systemctl restart mysql
```

From the primary VM, obtain the master status:

```bash
mysql -u root -p -e "SHOW MASTER STATUS\G"
```

Take note of:

* File: e.g. mysql-bin.000002
* Position: e.g. 157

Copy the SSL certificates to the standby VM. First, on the primary VM, copy certificates to a location that can be transferred:

```bash
# On mysql-primary, copy certificates to home directory for transfer
sudo cp /etc/mysql/ssl/ca-cert.pem /etc/mysql/ssl/client-cert.pem /etc/mysql/ssl/client-key.pem ~/
sudo chown ubuntu:ubuntu ~/ca-cert.pem ~/client-cert.pem ~/client-key.pem
```

Now transfer the certificates from your host machine:

```bash
multipass transfer mysql-primary:ca-cert.pem .
multipass transfer ca-cert.pem mysql-standby:
multipass transfer mysql-primary:client-cert.pem .
multipass transfer client-cert.pem mysql-standby:
multipass transfer mysql-primary:client-key.pem .
multipass transfer client-key.pem mysql-standby:
```

On the standby VM, move certificates to the SSL directory and set appropriate permissions:

```bash
# On mysql-standby, move certificates and set permissions
sudo mkdir -p /etc/mysql/ssl
sudo mv ~/ca-cert.pem ~/client-cert.pem ~/client-key.pem /etc/mysql/ssl/
sudo chmod 600 /etc/mysql/ssl/*.pem
sudo chown mysql:mysql /etc/mysql/ssl/*.pem
```

Dump the primary database to create a baseline for the replica:

```bash
# On mysql-primary, in the /home/ directory
mysqldump -u root -p --all-databases --master-data=2 --single-transaction --flush-logs --hex-blob > full.sql
```

Transfer the dumped database file to the standby VM:

```bash
multipass transfer mysql-primary:full.sql .
multipass transfer full.sql mysql-standby:
```

On the standby VM, import the dumped database:

```bash
mysql -u root -p < full.sql
```

Extract the replication log file and position from the full.sql file:

```bash
grep -i 'CHANGE MASTER' full.sql
```

You should see output like:
```
-- CHANGE MASTER TO MASTER_LOG_FILE='mysql-bin.000002', MASTER_LOG_POS=157;
```

Finally, configure the standby as a replica by providing the primary's IP, replication user credentials, and the extracted master log file and position. **Use the values from the mysqldump file, not the current master status**:

```bash
mysql -u root -p << 'EOF'
STOP SLAVE;
RESET SLAVE ALL;
CHANGE MASTER TO
  MASTER_HOST='PRIMARY_IP',
  MASTER_USER='replica',
  MASTER_PASSWORD='replica_pass',
  MASTER_LOG_FILE='mysql-bin.000002',
  MASTER_LOG_POS=157,
  MASTER_SSL=1,
  MASTER_SSL_CA='/etc/mysql/ssl/ca-cert.pem',
  MASTER_SSL_CERT='/etc/mysql/ssl/client-cert.pem',
  MASTER_SSL_KEY='/etc/mysql/ssl/client-key.pem';
START SLAVE;
EOF
```

**Important Notes**:
- Replace `PRIMARY_IP` with the actual IP address of your mysql-primary instance (you can get this with `multipass list`)
- Use the MASTER_LOG_FILE and MASTER_LOG_POS values from the mysqldump file (shown in the grep output above), not from the current master status
- The mysqldump file contains the exact replication coordinates from when the backup was created

## 4. Verify replication

 On the standby, check the slave status to ensure Slave_IO_Running and Slave_SQL_Running are both "Yes". You can do so as follows:

```bash
mysql -u root -p -e "SHOW SLAVE STATUS\G"
```

You can also verify data by selecting from testdb.t1:

```bash
mysql -u root -p -e "SELECT * FROM testdb.t1;"
```

To verify that SSL is being used for replication, check the Slave_IO_State and Slave_SSL_Cert fields in the slave status:

```bash
mysql -u root -p -e "SHOW SLAVE STATUS\G" | grep -E "Slave_IO_State|Slave_SSL_Cert"
```

You should see output similar to:
```
Slave_IO_State: Connecting to master
Slave_SSL_Cert: /etc/mysql/ssl/client-cert.pem
```

This confirms that the replication connection is using SSL.

## 5. Implement automatic failover

On mysql-standby, create the failover script /usr/local/bin/auto_failover.sh:

```bash
sudo nano /usr/local/bin/auto_failover.sh
```

Paste the following script content, replacing `PRIMARY_IP` with the actual IP address of `mysql-primary`:

```bash
#!/bin/bash
PRIMARY_HOST=PRIMARY_IP
ROOT_PASS="SecureRootPass123"

# Check if primary is available using root with password
mysqladmin -h "$PRIMARY_HOST" -u root -p"$ROOT_PASS" ping > /dev/null 2>&1
if [ $? -eq 0 ]; then
  echo "Primary is alive. No failover needed."
  exit 0
fi

echo "Primary appears down. Initiating failover..."

# Use root with password for local MySQL operations
mysql -u root -p"$ROOT_PASS" <<EOF
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

Edit root's `crontab` to run this script every minute:

```bash
sudo crontab -e
```

Add the following line to `crontab`:

```bash
* * * * * /usr/local/bin/auto_failover.sh >> /var/log/mysql-failover.log 2>&1
```

## 6. Automate Rejoining of the Old Primary

On mysql-primary, create the rejoin script /usr/local/bin/rejoin_cluster.sh:

```bash
sudo nano /usr/local/bin/rejoin_cluster.sh
```

Paste the following script content, replacing `STANDBY_IP` with the actual IP address of `mysql-standby`:

```bash
#!/bin/bash
NEW_PRIMARY_IP=STANDBY_IP
MYSQL_DATA_DIR="/var/lib/mysql"
DUMP_FILE="/tmp/resync.sql"
ROOT_PASS="SecureRootPass123"
REPL_USER="replica"
REPL_PASS="replica_pass"

[ -f "$MYSQL_DATA_DIR/.promoted_to_primary" ] && exit 0

# Use root user to connect to the new primary
mysqldump -h $NEW_PRIMARY_IP -u root -p"$ROOT_PASS" --all-databases --master-data=2 --single-transaction --flush-logs --hex-blob > $DUMP_FILE
[ $? -ne 0 ] && exit 1

systemctl stop mysql
rm -rf $MYSQL_DATA_DIR/*
mysqld --initialize-insecure
systemctl start mysql

# Restore the root password after reinitialization, then import the dump
mysql -u root <<EOF
ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY '$ROOT_PASS';
CREATE USER 'root'@'%' IDENTIFIED BY '$ROOT_PASS';
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
EOF

mysql -u root -p"$ROOT_PASS" < $DUMP_FILE

MASTER_LOG_FILE=$(grep 'MASTER_LOG_FILE' $DUMP_FILE | sed -E "s/.*MASTER_LOG_FILE='([^']+)'.*/\1/")
MASTER_LOG_POS=$(grep 'MASTER_LOG_POS' $DUMP_FILE | sed -E "s/.*MASTER_LOG_POS=([0-9]+).*/\1/")

mysql -u root -p"$ROOT_PASS" <<EOF
STOP SLAVE;
RESET SLAVE ALL;
CHANGE MASTER TO
  MASTER_HOST='$NEW_PRIMARY_IP',
  MASTER_USER='$REPL_USER',
  MASTER_PASSWORD='$REPL_PASS',
  MASTER_LOG_FILE='$MASTER_LOG_FILE',
  MASTER_LOG_POS=$MASTER_LOG_POS,
  MASTER_SSL=1,
  MASTER_SSL_CA='/etc/mysql/ssl/ca-cert.pem',
  MASTER_SSL_CERT='/etc/mysql/ssl/client-cert.pem',
  MASTER_SSL_KEY='/etc/mysql/ssl/client-key.pem';
START SLAVE;
EOF
```

This creates a permanent role reversal. The original primary doesn't get promoted back - it permanently becomes the slave of what used to be the standby.

This is actually a common pattern in high availability setups because:

* It's simpler to implement
* Avoids potential data conflicts
* The "failed" node has to prove it's healthy by successfully operating as a slave first
* Prevents split-brain scenarios

Make the script executable:

```bash
sudo chmod +x /usr/local/bin/rejoin_cluster.sh
```

Enable the script to run on boot by creating a systemd service file:

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

[Install]
WantedBy=multi-user.target
```

Enable the systemd service:

```bash
sudo systemctl enable mysql-rejoin.service
```

## 7. Validate High Availability

### Simulate failover

Simulate a failover by disabling zone1, which will shut down `mysql-primary`:

```bash
multipass zones-disable zone1
multipass stop mysql-primary --force # if unavailability is not supported for backend (functionally identical behavior to zones-disable)
```

Wait until the failover occurs. This may take up to two minutes. Then, on `mysql-standby`, check the following:

```bash
mysql -u root -p -e "SHOW SLAVE STATUS\G"  # should be empty
mysql -u root -p -e "INSERT INTO testdb.t1 VALUES (2);"
```

### Simulate rejoin:

Simulate a rejoin by starting `mysql-primary` again:

```bash
multipass start mysql-primary
```

On `mysql-primary`, check its slave status and data:

```bash
mysql -u root -p -e "SHOW SLAVE STATUS\G"
mysql -u root -p -e "SELECT * FROM testdb.t1;"
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

You have successfully established a Master–replica MySQL cluster across Multipass VMs with automatic failover when the primary fails and automatic recovery and rejoin when the primary returns.
