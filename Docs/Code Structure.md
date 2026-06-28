# ESP32-Smart-Home-Controller

```

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
