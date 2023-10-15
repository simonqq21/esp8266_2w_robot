#include <ArduinoJson.h>
#include <ESP8266WiFi.h> 
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h> 

// motor pins 
#define lmotor_pin 12
#define rmotor_pin 13
// light pin
#define lights_pin 14
// horn pin 
#define horn_pin 15 

// async web server
AsyncWebServer server(80); 
AsyncWebSocket ws("/ws"); 
StaticJsonDocument<70> inputDoc;
StaticJsonDocument<70> outputDoc;
char strData[70];

//static IP address configuration 
IPAddress local_IP(192,168,5,75);
IPAddress gateway(192,168,5,1);
IPAddress subnet(255,255,255,0);
//IPAddress primaryDNS(8,8,8,8);
//IPAddress secondaryDNS(8,8,4,4);
#define APMODE true

// status variables 
int lmotor_speed = 0;
int rmotor_speed = 0; 
boolean lights_state = false; 
boolean horn_state = false; 
uint32_t lastTimeUpdated = 0;
const int debounceDelay = 10; 

// wifi credentials
#define LOCAL_SSID "QUE-STARLINK"
#define LOCAL_PASS "Quefamily01259"

//for littlefs
File indexPage;  

void printWiFi() {
  Serial.println(" ");
  Serial.println("WiFi connected.");
  Serial.print("WiFi SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: "); 
  Serial.println(WiFi.localIP()); 
  long rssi = WiFi.RSSI(); 
  Serial.print("Signal strength (RSSI): "); 
  Serial.print(rssi);
  Serial.println(" dBm");
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp("new", (char*)data) == 0) {
      for (int i=0;i<LEDCOUNT;i++) {
        globalLEDIndex = i;
        sendLEDJSON();
      }
      for (int i=0;i<BTNCOUNT;i++) {
        globalBtnIndex = i; 
        sendBtnJSON();
      }
    }
    else {
      DeserializationError error = deserializeJson(inputDoc, (char*)data); 
      if (error) {
        Serial.print("deserializeJson failed: ");
        Serial.println(error.f_str());
      }
      else 
        Serial.println("deserializeJson success");
      strcpy(type, inputDoc["type"]);
      if (strcmp(type, TYPELED) == 0) {   
        controlLED();
      }
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200); 
  // littleFS 
  if (!LittleFS.begin()) {
    Serial.println("An error occured while mounting LittleFS.");
  }
  pinMode(lmotor_pin, OUTPUT); 
  pinMode(rmotor_pin, OUTPUT);
  pinMode(lights_pin, OUTPUT);
  pinMode(horn_pin, OUTPUT);
  // wifi 
  //  if ESP will start its own hotspot
  if (APMODE) {
    Serial.println("Starting AP");
    if (!WiFi.softAP("simon")) {
      Serial.println("Soft AP failed to configure.");
    }
    WiFi.softAPConfig(local_IP, gateway, subnet);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  }
  // else if ESP will connect to existing WiFi network
  else {
    Serial.print("Connecting to "); 
    Serial.println(LOCAL_SSID);
  
    if (!WiFi.config(local_IP, gateway, subnet)) {
      Serial.println("Station failed to configure.");
    }
    
    WiFi.begin(LOCAL_SSID, LOCAL_PASS); 
    while (WiFi.status() != WL_CONNECTED) {
      delay(500); 
      Serial.print(".");
    }
    //  print local IP address and start web server 
    printWiFi();
  }
  // initialize websocket 
  initWebSocket(); 

  // route for root web page 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false);});
  server.begin();
}

void loop() {
  ws.cleanupClients(); 

}
