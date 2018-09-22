Quick Docker image for luxd
---------------------------

Build docker image:
   
    docker/build.sh

Push docker image:

    docker/push.sh

Build docker for luxd
----------
A Docker configuration with luxd node by default.

    sudo apt install apt-transport-https ca-certificates curl software-properties-common; curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -; sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"; sudo apt-get update; sudo apt install docker-ce   
---------------------------------------------------        
    
    mkdir lux-mounted-data
    docker run --name lux -d \
     --env 'LUX_RPCUSER=rpciser' \
     --env 'LUX_RPCPASSWORD=rpcpassword' \
     --env 'LUX_TXINDEX=1' \
     --volume ~/lux-mounted-data:~/.lux \
     -p 9888:9888 \
     --publish 9888:9888 \
     luxcore/lux
----------------------------------------------------
Logs

    docker logs -f lux

----------------------------------------------------

## Configuration

Set your `lux.conf` file can be placed in the `lux-mounted data` dir.
Otherwise, a default `lux.conf` file will be automatically generated based
on environment variables passed to the container:

| name | default |
| ---- | ------- |
| BTC_RPCUSER | rpcuser |
| BTC_RPCPASSWORD | rpcpassword |
| BTC_RPCPORT | 9888 |
| BTC_RPCALLOWIP | ::/0 |
| BTC_RPCCLIENTTIMEOUT | 30 |
| BTC_DISABLEWALLET | 1 |
| BTC_TXINDEX | 0 |
| BTC_TESTNET | 0 |
| BTC_DBCACHE | 0 |
| BTC_ZMQPUBHASHTX | tcp://0.0.0.0:28333 |
| BTC_ZMQPUBHASHBLOCK | tcp://0.0.0.0:28333 |
| BTC_ZMQPUBRAWTX | tcp://0.0.0.0:28333 |
| BTC_ZMQPUBRAWBLOCK | tcp://0.0.0.0:28333 |


## Daemonizing

If you're daemonizing is to use Docker's 
[Daemonizing](https://docs.docker.com/config/containers/start-containers-automatically/#use-a-restart-policy),
but if you're insistent on using systemd, you could do something like

```
$ cat /etc/systemd/system/luxd.service

# luxd.service #######################################################################
[Unit]
Description=Lux
After=docker.service
Requires=docker.service

[Service]
ExecStartPre=-/usr/bin/docker kill lux
ExecStartPre=-/usr/bin/docker rm lux
ExecStartPre=/usr/bin/docker pull luxcore/lux
ExecStart=/usr/bin/docker run \
    --name lux \
    -p 9888:9888 \
    -p 9888:9888 \
    -v /data/luxd:/root/.lux \
    luxcore/lux
ExecStop=/usr/bin/docker stop lux
```
