#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

#define RELAY1 26   // Light
#define RELAY2 27   // Fan
#define RELAY3 25   // Socket 1
#define RELAY4 33   // Socket 2

// =====================
// WIFI + STATIC IP
// =====================
const char* ssid = "KIRTAN HOME";
const char* password = "Kirtan@1235";

IPAddress local_IP(192,168,1,36);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
IPAddress primaryDNS(8,8,8,8);
IPAddress secondaryDNS(8,8,4,4);

WebServer server(80);

bool state1 = false;
bool state2 = false;
bool state3 = false;
bool state4 = false;

// =====================
// UPDATE RELAYS
// =====================
void updateRelays()
{
  digitalWrite(RELAY1, state1 ? LOW : HIGH);
  digitalWrite(RELAY2, state2 ? LOW : HIGH);
  digitalWrite(RELAY3, state3 ? LOW : HIGH);
  digitalWrite(RELAY4, state4 ? LOW : HIGH);
}

// =====================
// STATUS STRING
// =====================
String getStatus()
{
  String s = "";

  s += state1 ? "1" : "0";
  s += state2 ? "1" : "0";
  s += state3 ? "1" : "0";
  s += state4 ? "1" : "0";

  return s;
}

// =====================
// INDIVIDUAL RELAYS
// =====================
void relay1()
{
  state1 = !state1;
  updateRelays();

  Serial.println("Light Toggled");

  server.send(200, "text/plain", "R1");
}

void relay2()
{
  state2 = !state2;
  updateRelays();

  Serial.println("Fan Toggled");

  server.send(200, "text/plain", "R2");
}

void relay3()
{
  state3 = !state3;
  updateRelays();

  Serial.println("Socket 1 Toggled");

  server.send(200, "text/plain", "R3");
}

void relay4()
{
  state4 = !state4;
  updateRelays();

  Serial.println("Socket 2 Toggled");

  server.send(200, "text/plain", "R4");
}

// =====================
// ALL ON
// =====================
void allOn()
{
  Serial.println("ALL ON RECEIVED");

  state1 = true;
  state2 = true;
  state3 = true;
  state4 = true;

  updateRelays();

  server.send(200, "text/plain", "ALL ON");
}

// =====================
// ALL OFF
// =====================
void allOff()
{
  Serial.println("ALL OFF RECEIVED");

  state1 = false;
  state2 = false;
  state3 = false;
  state4 = false;

  updateRelays();

  server.send(200, "text/plain", "ALL OFF");
}

// =====================
// STATUS API
// =====================
void statusAPI()
{
  server.send(200, "text/plain", getStatus());
}

// =====================
// HOME PAGE
// =====================
void homePage()
{
  String html = "";

  html += "<html><body>";
  html += "<h1>SMART HOME</h1>";

  html += "<p><a href='/r1'><button>Light</button></a></p>";
  html += "<p><a href='/r2'><button>Fan</button></a></p>";
  html += "<p><a href='/r3'><button>Socket 1</button></a></p>";
  html += "<p><a href='/r4'><button>Socket 2</button></a></p>";

  html += "<p><a href='/allon'><button>ALL ON</button></a></p>";
  html += "<p><a href='/alloff'><button>ALL OFF</button></a></p>";

  html += "<hr>";
  html += "<p>Status: ";
  html += getStatus();
  html += "</p>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// =====================
// SETUP
// =====================
void setup()
{
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);

  state1 = false;
  state2 = false;
  state3 = false;
  state4 = false;

  updateRelays();

if(!WiFi.config(local_IP, gateway, subnet,
                primaryDNS, secondaryDNS))
{
  Serial.println("Static IP Failed");
}

// Must be set BEFORE WiFi.begin() to take effect
WiFi.setHostname("Kirtan-Server");

WiFi.begin(ssid, password);

Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

WiFi.setHostname("Kirtan-Server");

ArduinoOTA.onStart([]() {
  Serial.println("OTA Start");
});

ArduinoOTA.onEnd([]() {
  Serial.println("\nOTA End");
});

ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  Serial.printf("Progress: %u%%\r", (progress * 100) / total);
});

ArduinoOTA.onError([](ota_error_t error) {
  Serial.printf("OTA Error[%u]\n", error);
});

ArduinoOTA.begin();

Serial.println("OTA Ready");

  server.on("/", homePage);
  server.on("/r1", relay1);
  server.on("/r2", relay2);
  server.on("/r3", relay3);
  server.on("/r4", relay4);
  server.on("/allon", allOn);
  server.on("/alloff", allOff);
  server.on("/status", statusAPI);

  server.begin();

  Serial.println("Server Started");
}

// =====================
// LOOP
// =====================
void loop()
{
  server.handleClient();
  ArduinoOTA.handle();
}