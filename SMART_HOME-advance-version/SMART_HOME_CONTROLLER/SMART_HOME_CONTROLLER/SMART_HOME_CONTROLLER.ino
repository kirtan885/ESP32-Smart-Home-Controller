#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <time.h>
#include <BleKeyboard.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================
// WIFI
// =====================
const char* ssid = "KIRTAN HOME";
const char* password = "Kirtan@1235";
String relayServer = "http://192.168.1.39";

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
const long  gmtOffset_sec = 19800;   // IST = UTC+5:30
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

// Debounce: last time each touch pad fired
unsigned long lastTouchTime[8] = {0};

// Per-pad debounce window (ms) — VOL+/VOL- get a shorter window
// so holding/repeating volume feels responsive, others stay at 250ms
// to avoid accidental double-fires on hotkeys/media buttons.
const unsigned long touchDebounce[8] = {
  250,  // 0 NEXT
  250,  // 1 WIN+1
  250,  // 2 PLAY
  250,  // 3 WIN+2
  250,  // 4 PREV
  250,  // 5 WIN+3
  80,   // 6 VOL-
  80    // 7 VOL+
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
// OLED BRIGHTNESS
// SSD1306 contrast values: 30=25%, 80=50%, 140=75%, 200=100%
// Do NOT use 255 — on many modules it turns the display off.
// =====================
uint8_t brightnessLevel = 140;   // default: 75%

// 4 safe steps: 30, 80, 140, 200
const uint8_t brightSteps[4] = {30, 80, 140, 200};
int brightIndex = 2;   // start at index 2 = 140

// =====================
// OLED POWER MANAGEMENT
// =====================
bool screenSleeping = false;

unsigned long lastActivity = 0;
const unsigned long SCREEN_TIMEOUT = 30000; // 30 sec

// =====================
// NON BLOCKING POPUP
// =====================
bool popupActive = false;
unsigned long popupEndTime = 0;

enum PopupType
{
  POPUP_TEXT,
  POPUP_SPOTIFY
};

PopupType popupType = POPUP_TEXT;
String popupMessage = "";

// =====================
// WIFI AUTO-RECONNECT
// =====================
unsigned long lastReconnectAttempt = 0;

// =====================
// MARKET MODE V4
// =====================

bool cryptoMode = false;

int marketScroll = 0;

const int TOTAL_ASSETS = 13;

// First 10 = crypto (fetched from CoinGecko).
// Last 3 = Indian indices — placeholders for now, filled by
// your own PC/server feed in a later step.
String assetName[TOTAL_ASSETS] =
{
  "BTC",
  "ETH",
  "BNB",
  "SOL",
  "XRP",
  "DOGE",
  "ADA",
  "LINK",
  "AVAX",
  "TRX",

  "NIFTY",
  "SENSEX",
  "NEXT50"
};

float assetPrice[TOTAL_ASSETS];
float assetChange[TOTAL_ASSETS];

// Set to 0 once market data is confirmed working — keeps Serial clean.
#define DEBUG_MARKET 1

unsigned long lastMarketUpdate = 0;
const unsigned long MARKET_REFRESH = 30000;

void setBrightness(uint8_t value)
{
  brightnessLevel = value;
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(brightnessLevel);
}

// =====================
// ACTIVITY / WAKE HANDLER
// =====================
void registerActivity()
{
  lastActivity = millis();

  if(screenSleeping)
  {
    display.ssd1306_command(SSD1306_DISPLAYON);
    screenSleeping = false;

    refreshScreen();
  }
}

// =====================
// CLOCK HELPER
// =====================
String getTimeString()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, 100))
    return "--:--";

  char buffer[12];
  strftime(buffer, sizeof(buffer), "%I:%M %p", &timeinfo);
  String t = String(buffer);
  if(t.charAt(0) == '0') t.remove(0,1);
  return t;
}

// =====================
// WIFI ICON — 8x8 bitmap, signal-bar style
// 3 bitmaps: strong / medium / weak signal, + an X for disconnected.
//
// Each row is 1 byte = 8 pixels wide.
// The bars grow from the bottom-right corner, classic phone-style.
//
//  strong (all 3 bars):          medium (2 bars):          weak (1 bar):
//  . . . . . . . .               . . . . . . . .           . . . . . . . .
//  . . . . . . . .               . . . . . . . .           . . . . . . . .
//  . 1 . . . . . .               . . . . . . . .           . . . . . . . .
//  . 1 . . . . . .               . . . . . . . .           . . . . . . . .
//  . 1 . 1 . . . .               . . . 1 . . . .           . . . . . . . .
//  . 1 . 1 . . . .               . . . 1 . . . .           . . . . . . . .
//  . 1 . 1 . 1 . .               . . . 1 . 1 . .           . . . . . . 1 .
//  . 1 . 1 . 1 . .               . . . 1 . 1 . .           . . . . . . 1 .
// =====================
const unsigned char wifi_strong[8] PROGMEM = {
  0b00000000,
  0b00000000,
  0b01000000,
  0b01000000,
  0b01010000,
  0b01010000,
  0b01010100,
  0b01010100
};
const unsigned char wifi_medium[8] PROGMEM = {
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00010000,
  0b00010000,
  0b00010100,
  0b00010100
};
const unsigned char wifi_weak[8] PROGMEM = {
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000100,
  0b00000100
};

void drawWiFiIcon(int x, int y)
{
  if(WiFi.status() != WL_CONNECTED)
  {
    // Small X — 8x8
    display.drawLine(x,   y,   x+7, y+7, WHITE);
    display.drawLine(x+7, y,   x,   y+7, WHITE);
    return;
  }

  int rssi = WiFi.RSSI();

  if(rssi > -65)
    display.drawBitmap(x, y, wifi_strong, 8, 8, WHITE);
  else if(rssi > -80)
    display.drawBitmap(x, y, wifi_medium, 8, 8, WHITE);
  else
    display.drawBitmap(x, y, wifi_weak,   8, 8, WHITE);
}

// =====================
// BLUETOOTH ICON — 8x8 bitmap
// Classic ᛒ rune shape.  When disabled: small X.
// When enabled but not connected: outline only.
// When connected: dot added to right side (drawn separately).
//
//  Bit layout (8 cols, 8 rows):
//  . . . 1 . . . .   0x10
//  . . 1 1 1 . . .   0x38  <- top-right arm
//  . 1 . 1 . 1 . .   0x54  <- cross
//  . . 1 1 1 . . .   0x38  <- middle-right arm
//  . . 1 1 1 . . .   0x38  <- middle-right arm (mirror)
//  . 1 . 1 . 1 . .   0x54  <- cross
//  . . 1 1 1 . . .   0x38  <- bottom-right arm
//  . . . 1 . . . .   0x10
// =====================
const unsigned char bt_icon[8] PROGMEM = {
  0x10,   // 00010000
  0x38,   // 00111000
  0x54,   // 01010100
  0x38,   // 00111000
  0x38,   // 00111000
  0x54,   // 01010100
  0x38,   // 00111000
  0x10    // 00010000
};

void drawBluetoothIcon(int x, int y)
{
  if(!bluetoothEnabled)
  {
    // Small X — 8x8
    display.drawLine(x,   y,   x+7, y+7, WHITE);
    display.drawLine(x+7, y,   x,   y+7, WHITE);
    return;
  }

  display.drawBitmap(x, y, bt_icon, 8, 8, WHITE);

  // Connected indicator: tiny dot to the right of the icon
  if(bleKeyboard.isConnected())
    display.fillRect(x+9, y+3, 2, 2, WHITE);
}

// =====================
// SHARED STATUS BAR
// Clock left | WiFi icon right-of-center | BT icon far right
// Icons are 8x8 px + 2px dot for BT = fits in top 8 rows safely.
// Positions: WiFi at x=104, BT at x=116 — leaves room for "12:59 PM"
// =====================
void drawStatusBar()
{
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 1);
  display.print(getTimeString());

  drawWiFiIcon(104, 0);
  drawBluetoothIcon(116, 0);

  display.drawLine(0, 10, 127, 10, WHITE);
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
  display.println(relayServer.substring(7));

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
// =====================
void drawCurrentPage()
{
  switch(currentPage)
  {
    case PAGE_DASHBOARD: drawDashboard();   break;
    case PAGE_DEVICES:   drawDevicesPage(); break;
    case PAGE_PHONE:     drawPhonePage();   break;
    case PAGE_SYSTEM:    drawSystemPage();  break;
  }
}

// =====================
// OLED: MARKET PAGE (V4)
// 4 rows visible at a time: name | price | 24h change
// Scroll with 'b' (up) / 'd' (down), toggle with 'c'
// =====================
void drawMarketPage()
{
  display.clearDisplay();

  drawStatusBar();

  int y = 14;

  for(int i=0;i<4;i++)
  {
    int idx = marketScroll + i;

    if(idx >= TOTAL_ASSETS)
      break;

    display.setCursor(0,y);

    display.print(assetName[idx]);

    display.setCursor(42,y);
    display.print(assetPrice[idx],0);

    display.setCursor(88,y);

    if(assetChange[idx] >= 0)
      display.print("+");

    display.print(assetChange[idx],1);
    display.print("%");

    y += 12;
  }

  display.display();
}

// =====================
// REFRESH HELPER
// Redraws whichever screen is "active" right now — the normal
// page, or the market overlay if crypto mode is on. Used by
// anything that wants to repaint without caring which mode we're in
// (popup expiry, sleep wake, relay status sync).
// =====================
void refreshScreen()
{
  if(cryptoMode)
    drawMarketPage();
  else
    drawCurrentPage();
}

// =====================
// POPUP (NON-BLOCKING)
// =====================
void showPopup(String msg, unsigned long duration = 1000)
{
  popupType = POPUP_TEXT;

  popupMessage = msg;
  popupEndTime = millis() + duration;
  popupActive = true;

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(10,25);
  display.println(msg);

  display.display();
}

// =====================
// POPUP MANAGER
// =====================
void updatePopup()
{
  if(popupActive &&
     millis() >= popupEndTime)
  {
    popupActive = false;
    refreshScreen();
  }
}

// =====================
// OLED AUTO SLEEP
// =====================
void checkScreenTimeout()
{
  if(!screenSleeping &&
     millis() - lastActivity > SCREEN_TIMEOUT)
  {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    screenSleeping = true;
  }
}

// =====================
// WIFI AUTO-RECONNECT
// =====================
void checkWiFiReconnect()
{
  if(WiFi.status() == WL_CONNECTED)
    return;

  if(millis() - lastReconnectAttempt > 10000)
  {
    lastReconnectAttempt = millis();

    Serial.println("Reconnecting WiFi...");
    WiFi.reconnect();

    configTime(
      gmtOffset_sec,
      daylightOffset_sec,
      ntpServer,
      ntpServerBackup
    );
  }
}

// =====================
// BRIGHTNESS POPUP (NON-BLOCKING)
// =====================
void showBrightnessPopup()
{
  int percent = 25 * (brightIndex + 1);

  showPopup(
    "BRT " + String(percent) + "%",
    600
  );
}

// =====================
// SPOTIFY ICON BITMAP
// =====================
const unsigned char spotify_icon[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x0F, 0xF0, 0x00,
  0x00, 0x3F, 0xFC, 0x00,
  0x00, 0xFF, 0xFF, 0x00,
  0x01, 0xFF, 0xFF, 0x80,
  0x03, 0xFF, 0xFF, 0xC0,
  0x07, 0xF0, 0x0F, 0xE0,
  0x0F, 0xC0, 0x03, 0xF0,
  0x0F, 0x87, 0xE1, 0xF0,
  0x1F, 0x3F, 0xFC, 0xF8,
  0x1E, 0x7F, 0xFE, 0x78,
  0x3F, 0xF8, 0x1F, 0xFC,
  0x3F, 0xE0, 0x07, 0xFC,
  0x3F, 0xC7, 0xE3, 0xFC,
  0x3F, 0x9F, 0xF9, 0xFC,
  0x3F, 0xBF, 0xFD, 0xFC,
  0x3F, 0xFC, 0x3F, 0xFC,
  0x3F, 0xF0, 0x0F, 0xFC,
  0x3F, 0xE3, 0xC7, 0xFC,
  0x1F, 0xCF, 0xF7, 0xF8,
  0x1F, 0xFF, 0xFF, 0xF8,
  0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0,
  0x07, 0xFF, 0xFF, 0xE0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x01, 0xFF, 0xFF, 0x80,
  0x00, 0xFF, 0xFF, 0x00,
  0x00, 0x3F, 0xFC, 0x00,
  0x00, 0x0F, 0xF0, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// =====================
// SPOTIFY ICON POPUP (NON-BLOCKING)
// =====================
void showMusicPopup(unsigned long duration)
{
  popupType = POPUP_SPOTIFY;

  popupActive = true;
  popupEndTime = millis() + duration;

  display.clearDisplay();
  display.drawBitmap(48, 16, spotify_icon, 32, 32, WHITE);
  display.display();
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
      refreshScreen();
    }
  }
  http.end();
}

// =====================
// SEND RELAY COMMAND
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
// ONE-TIME NETWORK DIAGNOSTIC (V4 debugging)
// Isolates which piece is actually broken — WiFi, NTP, or the
// CoinGecko HTTPS handshake — instead of guessing. Runs once at
// boot. Set DEBUG_MARKET to 0 above once everything checks out.
//
// Note on time sync: we use client.setInsecure() everywhere, which
// skips certificate date validation entirely — so an unsynced clock
// normally should NOT block the handshake here. If you see
// "TIME NOT SYNCED" but the /ping test below still returns 200,
// that confirms time sync isn't your blocker.
// =====================
void testCoinGeckoConnectivity()
{
#if DEBUG_MARKET
  Serial.println("---- Network diagnostic ----");

  Serial.print("WiFi status: ");
  Serial.println(WiFi.status());   // should be 3 (WL_CONNECTED)

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
  {
    Serial.println("TIME NOT SYNCED (NTP)");
  }
  else
  {
    Serial.print("Time: ");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.begin(client, "https://api.coingecko.com/api/v3/ping");

  int code = https.GET();
  Serial.print("CoinGecko /ping HTTP code: ");
  Serial.println(code);

  if(code > 0)
    Serial.println(https.getString());
  else
    Serial.println("(-1 = TLS handshake/connect failed before any HTTP response)");

  https.end();

  Serial.println("---- End diagnostic ----");
#endif
}

// =====================
// COINGECKO MARKET DATA (V4 — crypto only for now)
// Fetches top 10 coins by market cap in ONE call (not 10 separate
// requests) and matches each by symbol into assetName[0..9], so
// it doesn't break if CoinGecko's top-10 order shifts (e.g. a
// stablecoin briefly outranks one of these).
// Indices 10-12 (NIFTY/SENSEX/NEXT50) are left untouched here —
// those get filled by your own PC/server feed in the next step.
// =====================
void updateMarketData()
{
  if(WiFi.status() != WL_CONNECTED)
    return;

  // CoinGecko is https:// — plain http.begin(url) on ESP32 often fails
  // the TLS handshake (HTTP error -1) unless you hand it a secure client.
  WiFiClientSecure client;
  client.setInsecure();   // public read-only API — skip cert pinning

  HTTPClient http;

  http.begin(
    client,
    "https://api.coingecko.com/api/v3/coins/markets"
    "?vs_currency=usd"
    "&order=market_cap_desc"
    "&per_page=10"
    "&page=1"
    "&sparkline=false"
    "&price_change_percentage=24h"
  );

  int code = http.GET();

  Serial.print("CoinGecko HTTP code: ");
  Serial.println(code);

  if(code == 200)
  {
    String payload = http.getString();

#if DEBUG_MARKET
    Serial.print("Payload length: ");
    Serial.println(payload.length());
    Serial.println(payload);
#endif

    // The real response (all fields, 10 coins) easily runs 6-10KB —
    // 4096 was silently failing with NoMemory. 16KB is comfortable
    // headroom on ESP32's heap.
    DynamicJsonDocument doc(16384);

    DeserializationError err = deserializeJson(doc, payload);

    if(!err)
    {
      int matched = 0;

      for(JsonObject coin : doc.as<JsonArray>())
      {
        const char* symbol = coin["symbol"]; // e.g. "btc", "eth"
        if(!symbol)
          continue;

        for(int i = 0; i < 10; i++)
        {
          if(assetName[i].equalsIgnoreCase(symbol))
          {
            assetPrice[i]  = coin["current_price"]               | 0.0;
            assetChange[i] = coin["price_change_percentage_24h"] | 0.0;
            matched++;

#if DEBUG_MARKET
            Serial.print(assetName[i]);
            Serial.print(" = ");
            Serial.print(assetPrice[i]);
            Serial.print("  (");
            Serial.print(assetChange[i]);
            Serial.println("%)");
#endif
            break;
          }
        }
      }

      Serial.print("Matched ");
      Serial.print(matched);
      Serial.println(" / 10 coins");
    }
    else
    {
      Serial.print("CoinGecko JSON parse failed: ");
      Serial.println(err.c_str());
      Serial.print("Free heap: ");
      Serial.println(ESP.getFreeHeap());
    }
  }
  else
  {
    Serial.print("CoinGecko HTTP error: ");
    Serial.println(code);
    Serial.println("(-1 = TLS handshake failed; 403/429 = rate-limited/blocked)");
  }

  http.end();
}

// =====================
// SETUP
// =====================
void setup()
{
  Serial.begin(115200);

  bleKeyboard.begin();

  Wire.begin(21, 22);

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
  {
    Serial.println("OLED FAILED");
    while(true);
  }

  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);

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

  for(int i = 0; i < 8; i++)
    pinMode(touchPins[i], INPUT_PULLDOWN);

  setBrightness(140);   // safe default: 75% — do NOT use 255, it blanks some OLEDs

  testCoinGeckoConnectivity();

  updateStatus();
  drawCurrentPage();

  lastActivity = millis();
}

// =====================
// LOOP
// =====================
void loop()
{
  char key = keypad.getKey();
  if(key)
  {
    registerActivity();

    Serial.print("Key: ");
    Serial.println(key);

    switch(key)
    {
      case '1':
        sendRelay("r1");
        showPopup("LIGHT");
        break;

      case '2':
        sendRelay("r2");
        showPopup("FAN");
        break;

      case '3':
        sendRelay("r3");
        showPopup("SOCKET1");
        break;

      case '4':
        sendRelay("r4");
        showPopup("SOCKET2");
        break;

      case '5':
        sendRelay("allon");
        showPopup("ALL ON");
        break;

      case '6':
        sendRelay("alloff");
        showPopup("ALL OFF");
        break;

      // Key 7 = Brightness Up (steps: 25% → 50% → 75% → 100%)
      case '7':
        if(brightIndex < 3)
        {
          brightIndex++;
          setBrightness(brightSteps[brightIndex]);
        }
        showBrightnessPopup();
        break;

      // Key 8 = Brightness Down (steps: 100% → 75% → 50% → 25%)
      case '8':
        if(brightIndex > 0)
        {
          brightIndex--;
          setBrightness(brightSteps[brightIndex]);
        }
        showBrightnessPopup();
        break;

      case 'A':
        cryptoMode = false;
        currentPage = PAGE_DASHBOARD;
        drawCurrentPage();
        break;

      case 'B':
        cryptoMode = false;
        currentPage = PAGE_DEVICES;
        drawCurrentPage();
        break;

      case 'C':
        cryptoMode = false;
        currentPage = PAGE_PHONE;
        drawCurrentPage();
        break;

      case 'D':
        cryptoMode = false;
        currentPage = PAGE_SYSTEM;
        drawCurrentPage();
        break;

      case 'a':
        bluetoothEnabled = !bluetoothEnabled;
        showPopup(bluetoothEnabled ? "BT ON" : "BT OFF");
        break;

      // Market mode: 'b' scroll up, 'c' toggle in/out, 'd' scroll down
      case 'b':
        if(cryptoMode)
        {
          if(marketScroll > 0)
            marketScroll--;

          drawMarketPage();
        }
        break;

      case 'c':
        cryptoMode = !cryptoMode;

        if(cryptoMode)
        {
          updateMarketData();
          drawMarketPage();
        }
        else
        {
          drawCurrentPage();
        }
        break;

      case 'd':
        if(cryptoMode)
        {
          if(marketScroll < TOTAL_ASSETS - 4)
            marketScroll++;

          drawMarketPage();
        }
        break;
    }
  }

  // ---- Touch panel scan (debounced) ----
  for(int i = 0; i < 8; i++)
  {
    if(
        digitalRead(touchPins[i]) &&
        millis() - lastTouchTime[i] > touchDebounce[i]
    )
    {
      lastTouchTime[i] = millis();

      registerActivity();

      Serial.println(touchNames[i]);

      if(bluetoothEnabled && bleKeyboard.isConnected())
      {
        switch(i)
        {
          case 0:   // NEXT - silent
            bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
            delay(150);
            break;

          case 1:   // WIN+1 - Spotify icon popup
            bleKeyboard.press(KEY_LEFT_GUI);
            bleKeyboard.press('1');
            delay(50);
            bleKeyboard.releaseAll();
            showMusicPopup(800);
            break;

          case 2:   // PLAY - silent
            bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            delay(150);
            break;

          case 3:   // WIN+2
            bleKeyboard.press(KEY_LEFT_GUI);
            bleKeyboard.press('2');
            delay(50);
            bleKeyboard.releaseAll();
            showPopup("WIN2", 250);
            break;

          case 4:   // PREV - silent
            bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
            delay(150);
            break;

          case 5:   // WIN+3
            bleKeyboard.press(KEY_LEFT_GUI);
            bleKeyboard.press('3');
            delay(50);
            bleKeyboard.releaseAll();
            showPopup("WIN3", 250);
            break;

          case 6:   // VOL- - silent
            bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
            delay(55);
            break;

          case 7:   // VOL+ - silent
            bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
            delay(100);
            break;
        }
      }
      else
      {
        showPopup("NO BT", 250);
      }
    }
  }

  if(millis() - lastStatusCheck > 3000)
  {
    lastStatusCheck = millis();
    updateStatus();
  }

  if(
      cryptoMode &&
      millis() - lastMarketUpdate > MARKET_REFRESH
  )
  {
    lastMarketUpdate = millis();

    updateMarketData();

    drawMarketPage();
  }

  updatePopup();
  checkScreenTimeout();
  checkWiFiReconnect();
}