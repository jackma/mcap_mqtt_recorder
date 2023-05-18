# MCAP MQTT Recorder

## How to build

```bash
docker build . -t mcap_mqtt_recorder
```

# Run Mosquitto broker in local host machine

```bash
# Start MQTT broker on local machine with configuration to allow anonymous connection
mosquitto -v -c mosquitto_custom.conf

# Connect to local host broker with Linux docker
docker run -it --rm --net=host --name mcap_mqtt_recorder -v $(PWD):/output mcap_mqtt_recorder --topics "#:json" --stamp-property stamp --output /output/output.mcap

# Connect to local host broker with Mac docker
docker run -it --rm --name mcap_mqtt_recorder -v $(PWD):/output mcap_mqtt_recorder --host "tcp://host.docker.internal:1883" --topics "#:json" --stamp-property stamp --output /output/output.mcap

# Publish message on local machine
mosquitto_pub -t topic1 -m '{"test": 123}' --property PUBLISH user-property stamp 130
```

# Run Mosquitto broker in docker

```bash
docker run -it --rm --name mcap_mqtt_recorder -v $(PWD):/output mcap_mqtt_recorder --topics "#:json" --stamp-property stamp --output /output/output.mcap
docker exec -it mcap_mqtt_recorder mosquitto -v
docker exec -it mcap_mqtt_recorder mosquitto_pub -t topic1 -m '{"test": 123}' --property PUBLISH user-property stamp 130
```
