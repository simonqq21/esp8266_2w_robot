#include <ArduinoJson.h>
#include <ESP8266WiFi.h> 
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h> 

// motor pins 
#define LMOTOR_SPEED_PIN 4
#define RMOTOR_SPEED_PIN 5
#define LMOTOR_DIR_PIN1 12
#define LMOTOR_DIR_PIN2 13
#define RMOTOR_DIR_PIN1 14
#define RMOTOR_DIR_PIN2 15
// light pin
#define LIGHTS_PIN 0
// horn pin 
#define HORN_PIN 1

// async web server
AsyncWebServer server(80); 
AsyncWebSocket ws("/ws"); 
StaticJsonDocument<140> inputDoc;
StaticJsonDocument<140> outputDoc;
char strData[140];

//static IP address configuration 
IPAddress local_IP(192,168,5,75);
IPAddress gateway(192,168,5,1);
IPAddress subnet(255,255,255,0);
//IPAddress primaryDNS(8,8,8,8);
//IPAddress secondaryDNS(8,8,4,4);
#define APMODE true

// status variables 
int raw_lmotor, raw_rmotor;
int lmotor_power = 0;
int rmotor_power = 0; 
bool lmotor_dir = false; // true for forward rotation, false for backward rotation 
bool rmotor_dir = false;
bool lights_state = false; 
bool horn_state = false; 
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

// function that receives all JSON data from the controlling device
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    
    //    deserialize the JSON into a JSON object
    DeserializationError error = deserializeJson(inputDoc, (char*)data); 
    Serial.println((char*)data);
    if (error) {
      Serial.print("deserializeJson failed: ");
      Serial.println(error.f_str());
    }
    else 
      Serial.println("deserializeJson success");
      
    String commandType = inputDoc["type"];
    //    send status JSON
    if (commandType == "status") {
      sendStatusUpdate();
    }
    //  update motor power values
    else if (commandType == "motors") {
      controlMotors();
    }
    //    update headlight values
    else if (commandType == "lights") {
      controlLights();
    }
    //    update beeper values
    else if (commandType == "beep") {
      controlBeep();
    }
  }
}

 void sendStatusUpdate() {
   outputDoc.clear();
   outputDoc["type"] = "status";
   outputDoc["lightState"] = lights_state;
   outputDoc["beepState"] = horn_state;
   outputDoc["lmotor_power"] = lmotor_power;
   outputDoc["rmotor_power"] = rmotor_power;
   serializeJson(outputDoc, strData);
   ws.textAll(strData);
 }

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        // stop robot motors upon disconnection
        disconnectBrake();
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

void controlMotors() {
  raw_lmotor = inputDoc["lmotor_power"]; 
  raw_rmotor = inputDoc["rmotor_power"];

  // if emergency brake
  if (raw_lmotor < -255 && raw_rmotor < -255) 
    disconnectBrake();
  // else if nominal running state
  else {
    lmotor_power = abs(raw_lmotor);
    rmotor_power = abs(raw_rmotor); 
    lmotor_dir = (raw_lmotor > 0) ? true: false; 
    rmotor_dir = (raw_rmotor > 0) ? true: false;
    digitalWrite(LMOTOR_DIR_PIN1, lmotor_dir);
    digitalWrite(LMOTOR_DIR_PIN2, !lmotor_dir);
    digitalWrite(RMOTOR_DIR_PIN1, rmotor_dir);
    digitalWrite(RMOTOR_DIR_PIN2, !rmotor_dir);
    analogWrite(LMOTOR_SPEED_PIN, lmotor_power);
    analogWrite(RMOTOR_SPEED_PIN, rmotor_power);
  }
  Serial.print("left motor = ");
  Serial.println(lmotor_power);
  Serial.print("right motor = ");
  Serial.println(rmotor_power); 
  // send status update
  sendStatusUpdate();
} 

void disconnectBrake() {
  digitalWrite(LMOTOR_DIR_PIN1, false);
  digitalWrite(LMOTOR_DIR_PIN2, false);
  digitalWrite(RMOTOR_DIR_PIN1, false);
  digitalWrite(RMOTOR_DIR_PIN2, false);
  digitalWrite(LMOTOR_SPEED_PIN, false);
  digitalWrite(RMOTOR_SPEED_PIN, false);
}

void controlLights() {
  lights_state = inputDoc["lights_state"];
  digitalWrite(LIGHTS_PIN, lights_state); 
  sendStatusUpdate();
}

void controlBeep() {
  horn_state = inputDoc["horn_state"];
  digitalWrite(HORN_PIN, horn_state);
  sendStatusUpdate();
}

void setup() {
  Serial.begin(115200); 
  // littleFS 
  if (!LittleFS.begin()) {
    Serial.println("An error occured while mounting LittleFS.");
  }

  // motor controller output pins
  pinMode(LMOTOR_DIR_PIN1, OUTPUT); 
  pinMode(LMOTOR_DIR_PIN2, OUTPUT); 
  pinMode(RMOTOR_DIR_PIN1, OUTPUT); 
  pinMode(RMOTOR_DIR_PIN2, OUTPUT); 
  pinMode(LMOTOR_SPEED_PIN, OUTPUT);
  pinMode(RMOTOR_SPEED_PIN, OUTPUT);
  // lights and horn pin
  pinMode(LIGHTS_PIN, OUTPUT);
  pinMode(HORN_PIN, OUTPUT);
  
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
    // route for other files
  server.on("/jquery_min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/jquery_min.js", "text/javascript", false);});
    server.on("/forward.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/forward.svg", "image/svg+xml", false);});
    server.on("/backward.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/backward.svg", "image/svg+xml", false);});
    server.on("/left.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/left.svg", "image/svg+xml", false);});
    server.on("/right.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/right.svg", "image/svg+xml", false);});
    server.on("/brake.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/brake.svg", "image/svg+xml", false);});
    server.on("/lights.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/lights.svg", "image/svg+xml", false);});
    server.on("/horn.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/horn.svg", "image/svg+xml", false);});
  server.begin();
  
  disconnectBrake();
  delay(1000);
}

void loop() {
  ws.cleanupClients(); 
}
