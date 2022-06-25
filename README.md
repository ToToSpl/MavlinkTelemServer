# MavlinkTelemServer 
Server which using mavsdk takes newest mavlink telemetry and sends it on the port in JSON. Additional agility added by using Docker

## Building
I dont provide prebuild container, because I want to use it on arm platform and it is not ready yet.

```
docker build . -t mavsdk_simple_server
```

## Running

```
docker run -p 6969:6969 -p 14540:14540/udp -d --restart unless-stopped mavsdk_simple_server:latest
```
