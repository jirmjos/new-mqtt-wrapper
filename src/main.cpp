#include <Arduino.h>
#include <TasmotaMqtt.h>

#ifndef NAME
#define NAME "NEW-sensor-test"
#endif

#ifndef TOPIC
#define TOPIC "i3/program-me/NEW-sensor-test"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "i3detroit-wpa"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "i3detroit"
#endif

#ifndef MQTT_SERVER
#define MQTT_SERVER "10.13.0.22"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

TasmotaMqtt wrapper;
struct mqtt_wrapper_options mqtt_options;

void setup()
{
  Serial.begin(115200);


  //mqtt_options.connectedLoop = connectedLoop;
  //mqtt_options.callback = callback;
  //mqtt_options.connectSuccess = connectSuccess;
  mqtt_options.ssid = WIFI_SSID;
  mqtt_options.password = WIFI_PASSWORD;
  mqtt_options.mqtt_server = MQTT_SERVER;
  mqtt_options.mqtt_port = MQTT_PORT;
  mqtt_options.host_name = NAME;
  mqtt_options.deviceTopic = TOPIC;
  mqtt_options.debug_print = true;
  wrapper.start(&mqtt_options);
}

void loop()
{
  wrapper.loop();
}
