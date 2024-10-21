# Commands:
sudo phttp-bench-proxy --addr 10.0.1.8 --port 10000 --mac 08:c0:eb:b6:e8:05 --backlog 8192 --ho-addr 10.0.1.8 --ho-port 10001 --ho-backlog 64 --sw-addr 10.0.1.7 --sw-port 18080 --backends 10.0.1.9:10000 --tls --tls-crt ~/tls/svr.crt --tls-key ~/tls/svr.key --nworkers 1

sudo phttp-bench-backend --addr 10.0.1.9 --port 80 --mac 08:c0:eb:b6:c5:ad --backlog 8192 --ho-addr 10.0.1.9 --ho-port 10000 --ho-backlog 64 --sw-addr 10.0.1.7 --sw-port 18080 --proxy-addr 10.0.1.8 --proxy-port 10001 --tls --tls-crt /dev/null --tls-key /dev/null --nworkers 1


# TLS 
### setup
1. Client on 10.0.1.7, Server on 10.0.1.8:10000
2. The Prism server node's kernel version must be `5.9.0-rc1`, because Prism nodes (frontend and backend nodes) requires the net-next kernel to use getsockopt(TLS_RX).
3. check if creme.ko is installed properly: `lsmod | grep creme` 

### Prepare certificate files
1. Generate CA key
`openssl genrsa -out CA.key 2048`
2. Generate self-signed CA.pem (just presss Enter to all prompt questions)
`openssl req -x509 -new -nodes -key CA.key -days 3650 -out CA.pem`
3. Generate server key
`openssl genrsa -out svr.key 2048`
4. Generate server CSR (Certificate Signing Request) (Common Name (e.g. server FQDN or YOUR name) []:10.0.1.8:10000, otherwise just presss Enter to all prompt questions)
`openssl req -new -key svr.key -out svr.csr`
5. Generate server certificate signed by CA
`openssl x509 -req -in svr.csr -CA CA.pem -CAkey CA.key -CAcreateserial -out svr.crt -days 365`
6. Test
On node8: `python3 server.py`
On node7: `python3 client.py`
==> You will see the server can print out the HTTP request sent by the client
7. Now, run Prism with TLS
IMPORTANT: the permission of certificate files should be `-rw-r--r--` so that the Prism process can read those : 
`chmod 644 ./svr.key`
On node8: `sudo phttp-bench-proxy --addr 10.0.1.8 --port 10000 --mac 08:c0:eb:b6:e8:05 --backlog 8192 --ho-addr 10.0.1.8 --ho-port 10001 --ho-backlog 64 --sw-addr 10.0.1.7 --sw-port 18080 --backends 10.0.1.9:10000 --tls --tls-crt ./tls/svr.crt --tls-key ./tls/svr.key --nworkers 1`
On node9: `sudo phttp-bench-backend --addr 10.0.1.9 --port 80 --mac 08:c0:eb:b6:c5:ad --backlog 8192 --ho-addr 10.0.1.9 --ho-port 10000 --ho-backlog 64 --sw-addr 10.0.1.7 --sw-port 18080 --proxy-addr 10.0.1.8 --proxy-port 10001 --tls --tls-crt /dev/null --tls-key /dev/null --nworkers 1`
On node7: `python3 client.py`
==> You will see the HTTP request is handled proeperly

[Reference: https://blog.naver.com/username1103/222111281954]

### Redis Test
1. node8.conf: `tls-ca-cert-file CA.pem`, `tls-cert-file svr.crt`, `tls-key-file svr.key`
2. node8: `~/redis-server-tls ../config/node8_10001.conf`
3. node7: `~/redis-cli-tls --tls     --cert ~/tls2/svr.crt     --key ~/tls2/svr.key     --cacert ~/tls2/CA.pem     -h 10.0.1.8 -p 10000`



# Prism

## Setup on nsl cluster
$ ./run.sh 

(Follow nsl_cluster_setup.sh)


# Research prototype

All sources are published under Apache2 license unless there is no license text on top of the file.

## How to run benchmark applications

### Setup Vagrant environment

#### Requirements

- Vagrant 2.2.10 or above (only tested on Vagrant 2.2.10)
- [vagrant-libvirt](https://github.com/vagrant-libvirt/vagrant-libvirt)
- [vagrant-reload](https://github.com/aidanns/vagrant-reload)

In addition to this, you need approx 50GB of storage for VM images. If
your libvirt image volume (usually /var/lib/libvirt/images) doesn't have
enough volume, please prepare the capacity (e.g. Symlink the external
storage mount point to /var/lib/libvirt/images or change the image volume
location. c.f. 
https://www.unixarena.com/2015/12/linux-kvm-change-libvirt-vm-image-store-path.html/)

#### Procedure

```
# On top of this repo
vagrant up --provider libvirt --no-parallel
```

We observe sometimes the Vagrant fails to provision the nodes unexpectedly. In that case, destroy
the nodes and retrying usually worked for us.

Please do not remove `--no-parallel` since our Vagrant provisioner heavily use computing power of
our external storage.

### Setup Baremetal environment

See the Vagrantfile for required topology and provisioning procedure. The Linux distribution version
should be the same as the one used in the Vagrant base box.

### Run `phttp-bench` application

`phttp-bench` is an application which is useful for measuring the effect of the TCP handoff. Client specifies the sizeof the object to download by path like `/1000` . The unit is byte. Frontend just handoff the requests to the backends without any processing and the backends send the response with on-memory binary blob.

#### Setup switch

```
# On switch node
cd /home/vagrant/Prism-HTTP/switch
sudo ./bin/prism_switchd -s vale0 -I $(pwd)/include -f src/cpp/prism_switch.bpf.c -a 172.16.10.10:18080
```

#### Setup frontend

```
# On frontend1 node

# HTTP
sudo phttp-bench-proxy --addr 172.16.10.11 --port 80 --mac 02:00:00:00:00:01 --backlog 8192 --ho-addr 172.16.10.11 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --backends 172.16.10.12:8080,172.16.10.13:8080 --nworkers 1

# HTTPS (Please replace server.crt/key to your own one)
openssl req -x509 -sha256 -nodes -days 3650 -newkey rsa:2048 -subj /CN=localhost -keyout server.key -out server.crt
sudo phttp-bench-proxy --addr 172.16.10.11 --port 443 --mac 02:00:00:00:00:01 --backlog 8192 --ho-addr 172.16.10.11 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --backends 172.16.10.12:8080,172.16.10.13:8080 --tls --tls-crt server.crt --tls-key server.key --nworkers 1
```

#### Setup backends

```
# On backend1 node

# HTTP
sudo phttp-bench-backend --addr 172.16.10.12 --port 80 --mac 02:00:00:00:00:02 --backlog 8192 --ho-addr 172.16.10.12 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --nworkers 1

# HTTPS
sudo phttp-bench-backend --addr 172.16.10.12 --port 443 --mac 02:00:00:00:00:02 --backlog 8192 --ho-addr 172.16.10.12 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --tls --tls-crt /dev/null --tls-key /dev/null --nworkers 1

# On backend2 node

# HTTP
sudo phttp-bench-backend --addr 172.16.10.13 --port 80 --mac 02:00:00:00:00:03 --backlog 8192 --ho-addr 172.16.10.13 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --nworkers 1

# HTTPS
sudo phttp-bench-backend --addr 172.16.10.13 --port 443 --mac 02:00:00:00:00:03 --backlog 8192 --ho-addr 172.16.10.13 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --tls --tls-crt /dev/null --tls-key /dev/null --nworkers 1
```

#### Run test

```
# On client node
curl http://172.16.10.11/1000  # Download the 1K objects
```

### Run `phttp-kvs` application

`phttp-kvs` is a simple REST based object storage application. The object will be **sharded**.

#### Setup switch

```
# On switch node
cd /home/vagrant/Prism-HTTP/switch
sudo ./bin/prism_switchd -s vale0 -I $(pwd)/include -f src/cpp/prism_switch.bpf.c -a 172.16.10.10:18080
```

#### Setup frontend

```
# On frontend1 node

# HTTP
sudo phttp-kvs-proxy --addr 172.16.10.11 --port 80 --mac 02:00:00:00:00:01 --backlog 8192 --ho-addr 172.16.10.11 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --backends 172.16.10.12:8080,172.16.10.13:8080 --nworkers 1

# HTTPS (Please replace server.crt/key to your own one)
openssl req -x509 -sha256 -nodes -days 3650 -newkey rsa:2048 -subj /CN=localhost -keyout server.key -out server.crt
sudo phttp-kvs-proxy --addr 172.16.10.11 --port 443 --mac 02:00:00:00:00:01 --backlog 8192 --ho-addr 172.16.10.11 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --backends 172.16.10.12:8080,172.16.10.13:8080 --tls --tls-crt server.crt --tls-key server.key --nworkers 1
```

#### Setup backends

```
# Common
mkdir /tmp/prism-leveldb

# On backend1 node

# HTTP
sudo phttp-kvs-backend --addr 172.16.10.12 --port 80 --mac 02:00:00:00:00:02 --backlog 8192 --ho-addr 172.16.10.12 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --nworkers 1 --dbdir /tmp/prism-leveldb

# HTTPS
sudo phttp-kvs-backend --addr 172.16.10.12 --port 443 --mac 02:00:00:00:00:02 --backlog 8192 --ho-addr 172.16.10.12 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --tls --tls-crt /dev/null --tls-key /dev/null --nworkers 1 --dbdir /tmp/prism-leveldb

# On backend2 node

# HTTP
sudo phttp-kvs-backend --addr 172.16.10.13 --port 80 --mac 02:00:00:00:00:03 --backlog 8192 --ho-addr 172.16.10.13 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --nworkers 1 --dbdir /tmp/prism-leveldb

# HTTPS
sudo phttp-kvs-backend --addr 172.16.10.13 --port 443 --mac 02:00:00:00:00:03 --backlog 8192 --ho-addr 172.16.10.13 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --tls --tls-crt /dev/null --tls-key /dev/null --nworkers 1 --dbdir /tmp/prism-leveldb
```

#### Run test

```
# On client node
curl -X PUT http://172.16.10.11/foo -d "foofoofoo"  # PUT object
curl -X GET http://172.16.10.11/foo  # GET object
curl -X DELETE http://172.16.10.11/foo  # DELETE object
```

### Run `phttp-kvs-repl` application

`phttp-kvs-repl` is a simple REST based object storage application. The object will be **replicated**.

#### Setup switch

```
# On switch node
cd /home/vagrant/Prism-HTTP/switch
sudo ./bin/prism_switchd -s vale0 -I $(pwd)/include -f src/cpp/prism_switch.bpf.c -a 172.16.10.10:18080
```

#### Setup frontend

```
# On frontend1 node

# HTTP
sudo phttp-kvs-repl-proxy --addr 172.16.10.11 --port 80 --mac 02:00:00:00:00:01 --backlog 8192 --ho-addr 172.16.10.11 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --backends 172.16.10.12:8080,172.16.10.13:8080 --nworkers 1

# HTTPS (Please replace server.crt/key to your own one)
openssl req -x509 -sha256 -nodes -days 3650 -newkey rsa:2048 -subj /CN=localhost -keyout server.key -out server.crt
sudo phttp-kvs-repl-proxy --addr 172.16.10.11 --port 443 --mac 02:00:00:00:00:01 --backlog 8192 --ho-addr 172.16.10.11 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --backends 172.16.10.12:8080,172.16.10.13:8080 --tls --tls-crt server.crt --tls-key server.key --nworkers 1
```

#### Setup backends

```
# On backend1 node

# HTTP
sudo phttp-kvs-repl-backend --addr 172.16.10.12 --port 80 --mac 02:00:00:00:00:02 --backlog 8192 --ho-addr 172.16.10.12 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --next-server-addr 172.16.10.13 --next-server-port 8080 --nworkers 1

# HTTPS
sudo phttp-kvs-repl-backend --addr 172.16.10.12 --port 443 --mac 02:00:00:00:00:02 --backlog 8192 --ho-addr 172.16.10.12 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --tls --tls-crt /dev/null --tls-key /dev/null --next-server-addr 172.16.10.13 --next-server-port 8080 --nworkers 1

# On backend2 node

# HTTP
sudo phttp-kvs-repl-backend --addr 172.16.10.13 --port 80 --mac 02:00:00:00:00:03 --backlog 8192 --ho-addr 172.16.10.13 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --next-server-addr 0.0.0.0 --next-server-port 0 --nworkers 1

# HTTPS
sudo phttp-kvs-repl-backend --addr 172.16.10.13 --port 443 --mac 02:00:00:00:00:03 --backlog 8192 --ho-addr 172.16.10.13 --ho-port 8080 --ho-backlog 64 --sw-addr 172.16.10.10 --sw-port 18080 --proxy-addr 172.16.10.11 --proxy-port 8080 --tls --tls-crt /dev/null --tls-key /dev/null --next-server-addr 0.0.0.0 --next-server-port 0 --nworkers 1
```

#### Run test

```
# On client node
curl -X PUT http://172.16.10.11/foo -d "foofoofoo"  # PUT object
curl -X GET http://172.16.10.11/foo  # GET object
# Delete operation is not implemented yet
```
