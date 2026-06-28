#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <time.h>
#include <BleKeyboard.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================
// WIFI
// =====================
const char* ssid = "KIRTAN HOME";
const char* password = "Kirtan@1235";
String relayServer = "http://192.168.1.36";

// =====================
// BLE STREAM DECK
// =====================
BleKeyboard bleKeyboard("Kirtan Stream Deck");

bool bluetoothEnabled = true;

unsigned long lastVolUp = 0;
unsigned long lastVolDown = 0;

// =====================
// NTP / CLOCK
// =====================
const char* ntpServer = "pool.ntp.org";
const char* ntpServerBackup = "time.nist.gov";
const long  gmtOffset_sec = 19800;   // IST = UTC+5:30. Change if you're elsewhere.
const int   daylightOffset_sec = 0;

// =====================
// KEYPAD
// =====================
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','4'},
  {'5','6','7','8'},
  {'a','b','c','d'},
  {'A','B','C','D'}
};
byte rowPins[ROWS] = {13,4,14,27};
byte colPins[COLS] = {26,25,33,32};
Keypad keypad = Keypad(
  makeKeymap(keys),
  rowPins,
  colPins,
  ROWS,
  COLS
);

// =====================
// TOUCH PANEL
// NOTE: only GPIO15 (T3) and GPIO2 (T2) are native ESP32 touch pins.
// The rest assume external touch modules (e.g. TTP223) with digital outputs.
// =====================
const int touchPins[8] = {
  15, 2, 5, 18,
  19, 23, 16, 17
};

const char* touchNames[8] = {
  "NEXT",
  "WIN+1",
  "PLAY",
  "WIN+2",
  "PREV",
  "WIN+3",
  "VOL-",
  "VOL+"
};

// =====================
// PAGE SYSTEM
// =====================
int currentPage = 0;

#define PAGE_DASHBOARD 0
#define PAGE_DEVICES   1
#define PAGE_PHONE     2
#define PAGE_SYSTEM    3

// =====================
// RELAY STATUS
// =====================
bool lightState = false;
bool fanState = false;
bool socket1State = false;
bool socket2State = false;
unsigned long lastStatusCheck = 0;

// =====================
// CLOCK HELPER
// =====================
String getTimeString()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, 100))   // short timeout so this never blocks the loop for long
  {
    return "--:--";
  }
  char buffer[12];
  strftime(buffer, sizeof(buffer), "%I:%M %p", &timeinfo);
  String t = String(buffer);
  if(t.charAt(0) == '0') t.remove(0,1);   // "09:30 PM" -> "9:30 PM"
  return t;
}

// =====================
// SHARED STATUS BAR
// Time on the left, WiFi/Bluetooth indicator on the right. Used by every
// page so all four screens stay visually consistent.
// =====================
void drawStatusBar()
{
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0,0);
  display.print(getTimeString());

  display.setCursor(80,0);

  if(WiFi.status() == WL_CONNECTED)
    display.print("W");
  else
    display.print("X");

  if(bluetoothEnabled && bleKeyboard.isConnected())
    display.print(" B");
  else
    display.print(" -");

  display.drawLine(0,10,127,10,WHITE);
}

// =====================
// OLED: DASHBOARD
// =====================
void drawDashboard()
{
  display.clearDisplay();
  drawStatusBar();

  display.setCursor(0,18);
  display.print("Light   ");
  display.println(lightState ? "ON" : "OFF");

  display.setCursor(0,30);
  display.print("Fan     ");
  display.println(fanState ? "ON" : "OFF");

  display.setCursor(0,42);
  display.print("Socket1 ");
  display.println(socket1State ? "ON" : "OFF");

  display.setCursor(0,54);
  display.print("Socket2 ");
  display.println(socket2State ? "ON" : "OFF");

  display.display();
}

// =====================
// OLED: DEVICES PAGE
// =====================
void drawDevicesPage()
{
  display.clearDisplay();
  drawStatusBar();

  display.setCursor(0,18);
  display.print("1 Light");
  display.setCursor(70,18);
  display.print("5 ALL ON");

  display.setCursor(0,30);
  display.print("2 Fan");
  display.setCursor(70,30);
  display.print("6 ALL OFF");

  display.setCursor(0,42);
  display.println("3 Socket1");

  display.setCursor(0,54);
  display.println("4 Socket2");

  display.display();
}

// =====================
// OLED: PHONE PAGE
// =====================
void drawPhonePage()
{
  display.clearDisplay();
  drawStatusBar();

  display.setCursor(0,22);
  display.println("Relay IP:");

  display.setCursor(0,35);
  display.println(relayServer.substring(7));   // strips "http://"

  display.display();
}

// =====================
// OLED: SYSTEM PAGE
// =====================
void drawSystemPage()
{
  display.clearDisplay();
  drawStatusBar();

  display.setCursor(0,20);
  display.print("RSSI: ");
  display.print(WiFi.RSSI());
  display.println(" dBm");

  display.setCursor(0,32);
  display.print("WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");

  display.setCursor(0,44);
  display.print("IP: ");
  display.println(WiFi.localIP().toString());

  display.display();
}

// =====================
// PAGE ROUTER
// All redraws (status polling, touch, nav keys) go through this,
// so a background status refresh never yanks you back to Dashboard.
// =====================
void drawCurrentPage()
{
  switch(currentPage)
  {
    case PAGE_DASHBOARD: drawDashboard();    break;
    case PAGE_DEVICES:   drawDevicesPage();  break;
    case PAGE_PHONE:     drawPhonePage();    break;
    case PAGE_SYSTEM:    drawSystemPage();   break;
  }
}

// =====================
// POPUP
// =====================
void showPopup(String msg, unsigned long duration = 1000)
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10,25);
  display.println(msg);
  display.display();
  delay(duration);
}

// =====================
// MUSIC ICON POPUP (Touch 2 / Win+1)
// Generic music-note glyph, not the Spotify logo - that's a trademarked
// logo I can't reproduce. Swap in your own bitmap via display.drawBitmap()
// here if you create/source one yourself.
// =====================
void showMusicPopup(unsigned long duration)
{
  display.clearDisplay();

  display.fillCircle(40, 42, 8, WHITE);                  // note head
  display.fillRect(46, 14, 4, 30, WHITE);                // stem
  display.fillTriangle(50, 14, 70, 20, 50, 28, WHITE);   // flag

  display.display();
  delay(duration);
}

// =====================
// GET STATUS
// =====================
void updateStatus()
{
  if(WiFi.status() != WL_CONNECTED)
    return;
  HTTPClient http;
  http.begin(relayServer + "/status");
  int httpCode = http.GET();
  if(httpCode == 200)
  {
    String data = http.getString();
    if(data.length() >= 4)
    {
      lightState   = data[0] == '1';
      fanState     = data[1] == '1';
      socket1State = data[2] == '1';
      socket2State = data[3] == '1';
      drawCurrentPage();
    }
  }
  http.end();
}

// =====================
// TOGGLE RELAY / SEND COMMAND
// Used for both single relays (r1..r4) and group commands (allon/alloff)
// =====================
void sendRelay(String relay)
{
  if(WiFi.status() != WL_CONNECTED)
    return;
  HTTPClient http;
  http.begin(relayServer + "/" + relay);
  http.GET();
  http.end();
  delay(150);
  updateStatus();
}

// =====================
// SETUP
// =====================
void setup()
{
  Serial.begin(115200);

  bleKeyboard.begin();

  Wire.begin(21,22);

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
  {
    Serial.println("OLED FAILED");
    while(true);
  }

  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid,password);

  unsigned long wifiStart = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if(WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi Connected");
    Serial.println(WiFi.localIP());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, ntpServerBackup);
  }
  else
  {
    Serial.println("WiFi FAILED - continuing offline");
  }

  for(int i=0;i<8;i++)
  {
    pinMode(touchPins[i], INPUT_PULLDOWN);
  }

  updateStatus();
  drawCurrentPage();
}

// =====================
// LOOP
// =====================
void loop()
{
  char key = keypad.getKey();
  if(key)
  {
    Serial.print("Key: ");
    Serial.println(key);

    switch(key)
    {
      case '1':
        sendRelay("r1");
        showPopup("LIGHT");
        drawCurrentPage();
        break;
      case '2':
        sendRelay("r2");
        showPopup("FAN");
        drawCurrentPage();
        break;
      case '3':
        sendRelay("r3");
        showPopup("SOCKET1");
        drawCurrentPage();
        break;
      case '4':
        sendRelay("r4");
        showPopup("SOCKET2");
        drawCurrentPage();
        break;
      case '5':
        sendRelay("allon");
        showPopup("ALL ON");
        drawCurrentPage();
        break;
      case '6':
        sendRelay("alloff");
        showPopup("ALL OFF");
        drawCurrentPage();
        break;

      case 'A':
        currentPage = PAGE_DASHBOARD;
        drawCurrentPage();
        break;
      case 'B':
        currentPage = PAGE_DEVICES;
        drawCurrentPage();
        break;
      case 'C':
        currentPage = PAGE_PHONE;
        drawCurrentPage();
        break;
      case 'D':
        currentPage = PAGE_SYSTEM;
        drawCurrentPage();
        break;

      case 'a':
        bluetoothEnabled = !bluetoothEnabled;
        if(bluetoothEnabled)
          showPopup("BT ON");
        else
          showPopup("BT OFF");
        drawCurrentPage();
        break;
    }
  }

  // ---- Touch panel scan ----
  for(int i=0;i<8;i++)
  {
    if(digitalRead(touchPins[i]))
    {
      Serial.println(touchNames[i]);

      if(bluetoothEnabled && bleKeyboard.isConnected())
      {
        switch(i)
        {
          // Touch 1 - silent, no popup
          case 0:
            bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
            delay(150);
            break;

          // Touch 2 - icon popup, longer hold time
          case 1:
            bleKeyboard.press(KEY_LEFT_GUI);
            bleKeyboard.press('1');
            delay(50);
            bleKeyboard.releaseAll();
            showMusicPopup(800);
            drawCurrentPage();
            break;

          // Touch 3 - silent, no popup
          case 2:
            bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            delay(150);
            break;

          // Touch 4 - unchanged
          case 3:
            bleKeyboard.press(KEY_LEFT_GUI);
            bleKeyboard.press('2');
            delay(50);
            bleKeyboard.releaseAll();
            showPopup("WIN2", 250);
            drawCurrentPage();
            break;

          // Touch 5 - silent, no popup
          case 4:
            bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
            delay(150);
            break;

          // Touch 6 - unchanged
          case 5:
            bleKeyboard.press(KEY_LEFT_GUI);
            bleKeyboard.press('3');
            delay(50);
            bleKeyboard.releaseAll();
            showPopup("WIN3", 250);
            drawCurrentPage();
            break;

          // Touch 7 - silent, no popup
          case 6:
            bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
            delay(150);
            break;

          // Touch 8 - silent, no popup
          case 7:
            bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
            delay(150);
            break;
        }
      }
      else
      {
        showPopup("NO BT", 250);
        drawCurrentPage();
      }
    }
  }

  if(millis() - lastStatusCheck > 3000)
  {
    lastStatusCheck = millis();
    updateStatus();
  }
}
