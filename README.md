
# ESP32-Smart-Home-Controller

```
ESP32-Smart-Home-Controller/
│
├── README.md                     # Main project documentation
├── LICENSE                       # MIT License
├── .gitignore
├── CHANGELOG.md                  # Version history
├── CONTRIBUTING.md               # Contribution guide
│
├── Controller/
│   ├── Controller.ino
│   └── README.md                 # Controller explanation
│
├── Relay_Server/
│   ├── Relay_Server.ino
│   └── README.md                 # Relay server explanation
│
├── Wiring/
│   ├── Controller_Wiring.md
│   ├── Relay_Wiring.md
│   ├── Pinout_Table.md
│   ├── ESP32_Pinout.png
│   ├── Controller_Wiring.png
│   └── Relay_Wiring.png
│
├── Documentation/
│   ├── Project_Journey.md
│   ├── Project_Architecture.md
│   ├── Code_Structure.md
│   ├── Communication.md
│   ├── API.md
│   ├── Troubleshooting.md
│   ├── Libraries.md
│   └── Future_Plans.md
│
├── Images/
│   ├── Project.jpg
│   ├── Controller.jpg
│   ├── Relay.jpg
│   ├── OLED.jpg
│   ├── Dashboard.jpg
│   ├── Wiring.jpg
│   └── Demo.gif
│
├── Circuit_Diagram/
│   ├── SmartHome.fzz
│   ├── SmartHome.pdf
│   └── SmartHome.png
│
├── Flowcharts/
│   ├── SystemFlow.png
│   ├── ControllerFlow.png
│   └── RelayFlow.png
│
└── Assets/
    ├── Icons/
    └── Logos/
```

---

# README.md

## ESP32 Smart Home Controller

A dual ESP32 based smart home controller featuring an OLED dashboard, keypad interface, WiFi communication and HTTP-based relay control.

---

# Features

* Dual ESP32 Architecture
* OLED Dashboard
* 4×4 Matrix Keypad
* WiFi Communication
* HTTP Relay Server
* Live Relay Status
* Expandable Design
* Modular Codebase

---

# Hardware

| Device            | Quantity |
| ----------------- | -------: |
| ESP32 Dev Module  |        2 |
| SSD1306 OLED      |        1 |
| 4×4 Matrix Keypad |        1 |
| 4 Channel Relay   |        1 |
| Jumper Wires      |     Many |
| USB Cable         |        2 |

---

# Project Architecture

```
                WiFi Network
                     │
      ┌──────────────┴──────────────┐
      │                             │
Controller ESP32              Relay ESP32
(Keypad + OLED)              (Relay Server)
      │                             │
      └────────HTTP Requests────────┘
```

---

# Controller Responsibilities

* Read keypad
* Display OLED dashboard
* Send HTTP commands
* Display relay status

---

# Relay Server Responsibilities

* Receive HTTP requests
* Toggle relays
* Return current status

---

# API

GET /status

Returns

```
1010
```

Meaning

```
Light    ON
Fan      OFF
Socket1  ON
Socket2  OFF
```

---

GET /r1

Toggle Relay 1

GET /r2

Toggle Relay 2

GET /r3

Toggle Relay 3

GET /r4

Toggle Relay 4

GET /allon

Turn everything ON

GET /alloff

Turn everything OFF

---

# Wiring Guide

## Controller ESP32

OLED

| OLED | ESP32  |
| ---- | ------ |
| VCC  | 3.3V   |
| GND  | GND    |
| SDA  | GPIO21 |
| SCL  | GPIO22 |

Keypad

| Row | GPIO |
| --- | ---- |
| R1  | 13   |
| R2  | 4    |
| R3  | 14   |
| R4  | 27   |

| Column | GPIO |
| ------ | ---- |
| C1     | 26   |
| C2     | 25   |
| C3     | 33   |
| C4     | 32   |

---

## Relay ESP32

| Relay  | GPIO |
| ------ | ---- |
| Relay1 | 26   |
| Relay2 | 27   |
| Relay3 | 14   |
| Relay4 | 12   |

---

# Code Structure

Controller.ino

```
setup()

↓

Connect WiFi

↓

Initialize OLED

↓

Initialize Keypad

↓

Loop()

↓

Read Keypad

↓

Send HTTP Request

↓

Receive Relay Status

↓

Update OLED
```

Relay_Server.ino

```
setup()

↓

Connect WiFi

↓

Start WebServer

↓

Wait For Request

↓

Toggle Relay

↓

Return Status
```

---

# Project Journey

Version 1

* Learned ESP32 basics
* Tested WiFi

Version 2

* Added OLED

Version 3

* Added keypad

Version 4

* Added relay server

Version 5

* Added dashboard

Version 6

* Added BLE media controller

Version 7

* Added touch buttons

Version 8

* Added crypto dashboard

Version 9

* Simplified project
* Removed unnecessary features
* Improved stability
* Rebuilt from scratch

---

# Future Roadmap

* MQTT
* Home Assistant
* OTA Updates
* Mobile App
* Weather Widget
* Voice Assistant
* Zigbee
* Matter Support
* ESP-NOW Communication
* Secure Authentication

---

# Troubleshooting

OLED blank

* Check I²C address
* Verify SDA/SCL wiring
* Run I²C scanner

WiFi won't connect

* Verify SSID/password
* Ensure both ESP32s are on the same network

Relay not switching

* Test `/r1` in a browser
* Verify relay GPIO wiring

Keypad delay

* Remove blocking delays
* Check WiFi timeout
* Use Flash Frequency = 40 MHz

---

# Libraries

Adafruit SSD1306

Adafruit GFX

Keypad

WiFi

HTTPClient

WebServer

ArduinoJson

BleKeyboard (optional)

---

# License

MIT License

```
```
