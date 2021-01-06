#include <config.h>
#include <ArduinoOTA.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>

#define USE_SERIAL Serial

const char *wlan_ssid = WLAN_SSID;
const char *wlan_password = WLAN_PASSWORD;

const char *socket_server = SOCKET_SERVER_ADDRESS;
const int socket_port = SOCKET_SERVER_PORT;

int sensor_value = 0;
int threshold = MICROPHONE_THRESHOLD;
int abs_value = 0;

ESP8266WiFiMulti WiFiMulti;
SocketIOclient socketIO;

void handleOTASetup() {
  ArduinoOTA.setHostname(SMART_RING_HOST_NAME);
  ArduinoOTA.setPassword(SMART_RING_OTA_PASSWORD);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case sIOtype_DISCONNECT:
            USE_SERIAL.printf("[IOc] Disconnected!\n");
            break;
        case sIOtype_CONNECT:
            USE_SERIAL.printf("[IOc] Connected to url: %s\n", payload);

            // join default namespace (no auto join in Socket.IO V3)
            socketIO.send(sIOtype_CONNECT, "/");
            break;
        case sIOtype_EVENT:
            USE_SERIAL.printf("[IOc] get event: %s\n", payload);
            break;
        case sIOtype_ACK:
            USE_SERIAL.printf("[IOc] get ack: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_ERROR:
            USE_SERIAL.printf("[IOc] get error: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_BINARY_EVENT:
            USE_SERIAL.printf("[IOc] get binary: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_BINARY_ACK:
            USE_SERIAL.printf("[IOc] get binary ack: %u\n", length);
            hexdump(payload, length);
            break;
    }
}

void setup() {
  // USE_SERIAL.begin(921600);
  USE_SERIAL.begin(9600);

  //Serial.setDebugOutput(true);
  USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  // disable AP
  if (WiFi.getMode() & WIFI_AP) {
    WiFi.softAPdisconnect(true);
  }

  WiFiMulti.addAP(wlan_ssid, wlan_password);

  //WiFi.disconnect();
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  String ip = WiFi.localIP().toString();
  USE_SERIAL.printf("[SETUP] WiFi Connected %s\n", ip.c_str());

  handleOTASetup();

  // server address, port and URL
  socketIO.begin(socket_server, socket_port);

  // event handler
  socketIO.onEvent(socketIOEvent);
}

void sendMessage(String sensorData, String absSensorData) {
  // creat JSON message for Socket.IO (event)
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();

  // add evnet name
  // Hint: socket.on('event_name', ....
  array.add("message");

  // add payload (parameters) for the event
  JsonObject param1 = array.createNestedObject();
  param1["msg"] = "Bell Ringed!";
  param1["sensor"] = sensorData;
  param1["absSensor"] = absSensorData;

  // JSON to String (serializion)
  String output;
  serializeJson(doc, output);

  // Send event
  socketIO.sendEVENT(output);

  // Print JSON for debugging
  USE_SERIAL.println(output);
}

void loop() {
  ArduinoOTA.handle();
  socketIO.loop();

  // value of the mic
  sensor_value = analogRead(A0);
  USE_SERIAL.println(sensor_value);
  // absolute value of the mic
  abs_value = abs(sensor_value - threshold);

  //Serial.println("VAL: " + String(sensor_value));
  //Serial.println("ABS: " + String(abs_value));

  if (abs_value >= 35) {
    sendMessage(String(sensor_value), String(abs_value));
    // Delay of 10 seconds to let bell finish ringing
    delay(2 * 1000);
  }

  delay(100);
}
