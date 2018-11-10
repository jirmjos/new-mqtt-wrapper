/*
  TasmotaMqtt.cpp - Wrapper for mqtt for esp8266 by Tuan PM for Tasmota

  Copyright (C) 2018 Theo Arends and Ingo Randolf

  This library is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.        See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.        If not, see <http://www.gnu.org/licenses/>.
*/

#include "TasmotaMqtt.h"

#include "JustWifi.h"

#include "user_interface.h"
#include "osapi.h"
#include "os_type.h"

/*********************************************************************************************\
 * Prerequisite
 *
 * Copy .c and .h files from https://github.com/tuanpmt/esp_mqtt folder mqtt to folder mqtt
 * - Replace BOOL with bool
 * - Remove variables certificate and private_key from file mqtt.c
 * - Add file user_config.h with default defines for
 *                 MQTT_BUF_SIZE 256, MQTT_RECONNECT_TIMEOUT 5, QUEUE_BUFFER_SIZE 2048 and PROTOCOL_NAMEv311
\*********************************************************************************************/

/*********************************************************************************************\
 * Mqtt internal callbacks
\*********************************************************************************************/

static void mqttConnectedCb(uint32_t *args)
{
        MQTT_Client* client = (MQTT_Client*)args;
        TasmotaMqtt* _this = (TasmotaMqtt*)client->user_data;
        Serial.println("MQTT connected");
        if (_this) _this->_onMqttConnectedCb();
}

static void mqttDisconnectedCb(uint32_t *args)
{
        MQTT_Client* client = (MQTT_Client*)args;
        TasmotaMqtt* _this = (TasmotaMqtt*)client->user_data;
        if (_this && _this->onMqttDisconnectedCb) _this->onMqttDisconnectedCb();
}

static void mqttPublishedCb(uint32_t *args)
{
        MQTT_Client* client = (MQTT_Client*)args;
        TasmotaMqtt* _this = (TasmotaMqtt*)client->user_data;
        if (_this && _this->onMqttPublishedCb) _this->onMqttPublishedCb();
}

static void mqttTimeoutCb(uint32_t *args)
{
        MQTT_Client* client = (MQTT_Client*)args;
        TasmotaMqtt* _this = (TasmotaMqtt*)client->user_data;
        if (_this && _this->onMqttTimeoutCb) _this->onMqttTimeoutCb();
}

static void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
        MQTT_Client* client = (MQTT_Client*)args;
        TasmotaMqtt* _this = (TasmotaMqtt*)client->user_data;
        if (_this) _this->_onMqttDataCb(topic, topic_len, data, data_len);
}

//wifi internal callback
static void wifiCb(justwifi_messages_t code, char * parameter, void* user_data)
{
        TasmotaMqtt* _this = (TasmotaMqtt*)user_data;
        if (_this) _this->_onWifiCb(code, parameter);
}

static void infoWifi() {
    if (WiFi.isConnected()) {
        uint8_t * bssid = WiFi.BSSID();

        Serial.printf("[WIFI] MODE STA -------------------------------------\n");
        Serial.printf("[WIFI] SSID  %s\n", WiFi.SSID().c_str());
        Serial.printf("[WIFI] BSSID %02X:%02X:%02X:%02X:%02X:%02X\n",
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]
        );
        Serial.printf("[WIFI] CH    %d\n", WiFi.channel());
        Serial.printf("[WIFI] RSSI  %d\n", WiFi.RSSI());
        Serial.printf("[WIFI] IP    %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WIFI] MAC   %s\n", WiFi.macAddress().c_str());
        Serial.printf("[WIFI] GW    %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("[WIFI] MASK  %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("[WIFI] DNS   %s\n", WiFi.dnsIP().toString().c_str());
        #if defined(ARDUINO_ARCH_ESP32)
            Serial.printf("[WIFI] HOST  %s\n", WiFi.getHostname());
        #else
            Serial.printf("[WIFI] HOST  %s\n", WiFi.hostname().c_str());
        #endif
        Serial.printf("[WIFI] ----------------------------------------------\n");
    }

    if (WiFi.getMode() & WIFI_AP) {
        Serial.printf("[WIFI] MODE AP --------------------------------------\n");
        Serial.printf("[WIFI] SSID  %s\n", jw.getAPSSID().c_str());
        Serial.printf("[WIFI] IP    %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("[WIFI] MAC   %s\n", WiFi.softAPmacAddress().c_str());
        Serial.printf("[WIFI] ----------------------------------------------\n");
    }
}

/*********************************************************************************************\
 * TasmotaMqtt class implementation
\*********************************************************************************************/

TasmotaMqtt::TasmotaMqtt() :
        onMqttConnectedCb(0),
        onMqttDisconnectedCb(0),
        onMqttPublishedCb(0),
        onMqttTimeoutCb(0),
        onMqttDataCb(0)
{
}

TasmotaMqtt::~TasmotaMqtt()
{
        MQTT_DeleteClient(&mqttClient);
}

void TasmotaMqtt::_onMqttConnectedCb()
{
        Serial.println("MQTT connected");
        char lwtTopic[512];
        sprintf(lwtTopic, "tele/%s/LWT", options->deviceTopic);
        Publish(lwtTopic, "Online", true);
        if (onMqttConnectedCb) onMqttConnectedCb();
}

void TasmotaMqtt::_onWifiCb(justwifi_messages_t code, char * parameter)
{
        switch(code) {
                case MESSAGE_TURNING_OFF:
                        Serial.printf("[WIFI] Turning OFF\n");
                        break;
                case MESSAGE_TURNING_ON:
                        Serial.printf("[WIFI] Turning ON\n");
                        break;
        // -------------------------------------------------------------------------
                case MESSAGE_SCANNING:
                        Serial.printf("[WIFI] Scanning\n");
                        break;
                case MESSAGE_SCAN_FAILED:
                        Serial.printf("[WIFI] Scan failed\n");
                        break;
                case MESSAGE_NO_NETWORKS:
                        Serial.printf("[WIFI] No networks found\n");
                        break;
                case MESSAGE_NO_KNOWN_NETWORKS:
                        Serial.printf("[WIFI] No known networks found\n");
                        break;
                case MESSAGE_FOUND_NETWORK:
                        Serial.printf("[WIFI] %s\n", parameter);
                        break;
        // -------------------------------------------------------------------------
                case MESSAGE_CONNECTING:
                        Serial.printf("[WIFI] Connecting to %s\n", parameter);
                        break;
                case MESSAGE_CONNECT_WAITING:
        // too much noise
                        break;
                case MESSAGE_CONNECT_FAILED:
                        Serial.printf("[WIFI] Could not connect to %s\n", parameter);
                        break;
                case MESSAGE_CONNECTED:
                        infoWifi();
                        //connect to mqtt
                        Connect();
                        break;
                case MESSAGE_DISCONNECTED:
                        Serial.printf("[WIFI] Disconnected\n");
                        break;
        // -------------------------------------------------------------------------
                case MESSAGE_ACCESSPOINT_CREATED:
                        infoWifi();
                        break;
                case MESSAGE_ACCESSPOINT_DESTROYED:
                        Serial.printf("[WIFI] Disconnecting access point\n");
                        break;
                case MESSAGE_ACCESSPOINT_CREATING:
                        Serial.printf("[WIFI] Creating access point\n");
                        break;
                case MESSAGE_ACCESSPOINT_FAILED:
                        Serial.printf("[WIFI] Could not create access point\n");
                        break;
        // ------------------------------------------------------------------------
                case MESSAGE_WPS_START:
                        Serial.printf("[WIFI] WPS started\n");
                        break;
                case MESSAGE_WPS_SUCCESS:
                        Serial.printf("[WIFI] WPS succeded!\n");
                        break;
                case MESSAGE_WPS_ERROR:
                        Serial.printf("[WIFI] WPS failed\n");
                        break;
        // ------------------------------------------------------------------------
                case MESSAGE_SMARTCONFIG_START:
                        Serial.printf("[WIFI] Smart Config started\n");
                        break;
                case MESSAGE_SMARTCONFIG_SUCCESS:
                        Serial.printf("[WIFI] Smart Config succeded!\n");
                        break;
                case MESSAGE_SMARTCONFIG_ERROR:
                        Serial.printf("[WIFI] Smart Config failed\n");
                        break;
        }
}
void TasmotaMqtt::start(struct mqtt_wrapper_options* newOptions)
{
        options = newOptions;
        // Set WIFI hostname (otherwise it would be ESP-XXXXXX)
        jw.setHostname(options->host_name);

        // Callbacks
        jw.user_data = (void*)this;
        jw.subscribe(wifiCb);

        // -------------------------------------------------------------------------

        // AP mode only as fallback
        jw.enableAP(false);
        //jw.enableAPFallback(true);

        // -------------------------------------------------------------------------

        // Enable STA mode (connecting to a router)
        jw.enableSTA(true);

        // Configure it to scan available networks and connect in order of dBm
        jw.enableScan(true);

        // Clean existing network configuration
        jw.cleanNetworks();

        // Add a network with password
        Serial.print("scanning for ");
        Serial.println(options->ssid);
        if(options->password) {
                jw.addNetwork(options->ssid, options->password);
        } else {
                jw.addNetwork(options->ssid);
        }

        InitConnection(options->mqtt_server, options->mqtt_port);
        InitClient(options->host_name, "", "");
        char lwtTopic[512];
        sprintf(lwtTopic, "tele/%s/LWT", options->deviceTopic);
        InitLWT(lwtTopic, "Offline");

        Subscribe(options->deviceTopic);
}

void TasmotaMqtt::loop()
{
        jw.loop();
}

void TasmotaMqtt::InitConnection(const char* host, uint32_t port, uint8_t security)
{
        MQTT_InitConnection(&mqttClient, (uint8_t*)host, port, security);

        // set user data
        mqttClient.user_data = (void*)this;

        MQTT_OnConnected(&mqttClient, mqttConnectedCb);
        MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
        MQTT_OnPublished(&mqttClient, mqttPublishedCb);
        MQTT_OnTimeout(&mqttClient, mqttTimeoutCb);
        MQTT_OnData(&mqttClient, mqttDataCb);
}

void TasmotaMqtt::InitClient(const char* client_id, const char* client_user, const char* client_pass, uint32_t keep_alive_time, uint8_t clean_session)
{
        MQTT_InitClient(&mqttClient, (uint8_t*)client_id, (uint8_t*)client_user, (uint8_t*)client_pass, keep_alive_time, clean_session);
}

void TasmotaMqtt::DeleteClient()
{
        MQTT_DeleteClient(&mqttClient);
}

void TasmotaMqtt::InitLWT(const char* will_topic, const char* will_msg, uint8_t will_qos, bool will_retain)
{
        MQTT_InitLWT(&mqttClient, (uint8_t*)will_topic, (uint8_t*)will_msg, will_qos, (uint8_t)will_retain);
}

void TasmotaMqtt::OnConnected( void (*function)(void) )
{
        onMqttConnectedCb = function;
}

void TasmotaMqtt::OnDisconnected( void (*function)(void) )
{
        onMqttDisconnectedCb = function;
}

void TasmotaMqtt::OnPublished( void (*function)(void) )
{
        onMqttPublishedCb = function;
}

void TasmotaMqtt::OnTimeout( void (*function)(void) )
{
        onMqttTimeoutCb = function;
}

void TasmotaMqtt::OnData( void (*function)(char*, uint8_t*, unsigned int) )
{
        onMqttDataCb = function;
}

bool TasmotaMqtt::Subscribe(const char* topic, uint8_t qos)
{
        return MQTT_Subscribe(&mqttClient, (char*)topic, qos);
}

bool TasmotaMqtt::Unsubscribe(const char* topic)
{
        return MQTT_UnSubscribe(&mqttClient, (char*)topic);
}

void TasmotaMqtt::Connect()
{
        MQTT_Connect(&mqttClient);
}

void TasmotaMqtt::Connect(const char* client_id, const char* client_user, const char* client_pass, const char* will_topic, const char* will_msg, uint8_t will_qos, bool will_retain)
{
        MQTT_InitClient(&mqttClient, (uint8_t*)client_id, (uint8_t*)client_user, (uint8_t*)client_pass, MQTT_KEEPALIVE, 1);
        MQTT_InitLWT(&mqttClient, (uint8_t*)will_topic, (uint8_t*)will_msg, will_qos, (uint8_t)will_retain);
        MQTT_Connect(&mqttClient);
}

void TasmotaMqtt::Disconnect()
{
        MQTT_Disconnect(&mqttClient);
}

bool TasmotaMqtt::Publish(const char* topic, const char* data, bool retain, int qos)
{
        return MQTT_Publish(&mqttClient, topic, data, strlen(data), qos, (int)retain);
}

bool TasmotaMqtt::Connected()
{
        return (mqttClient.connState > TCP_CONNECTED);
}

/*********************************************************************************************/

void TasmotaMqtt::_onMqttDataCb(const char* topic, uint32_t topic_len, const char* data, uint32_t data_len)
{
        char topic_copy[topic_len +1];

        memcpy(topic_copy, topic, topic_len);
        topic_copy[topic_len] = 0;
        if (0 == data_len) data = (const char*)&topic_copy + topic_len;
        onMqttDataCb((char*)topic_copy, (byte*)data, data_len);
}
