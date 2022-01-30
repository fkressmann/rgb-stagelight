#include "display.h"
#include "dmx.h"
#include "ota.h"
#include <Arduino.h>
#include <ArtnetWifi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsync_WiFiManager.h>

#define GPIO_LED_R 14
#define PWM_CHANNEL_R 0
#define GPIO_LED_G 12
#define PWM_CHANNEL_G 1
#define GPIO_LED_B 13
#define PWM_CHANNEL_B 2

#define GPIO_CASE_FAN 5
#define GPIO_FAN_OUT 25
#define PWM_CHANNEL_FAN_OUT 3
#define GPIO_FAN_1_IN 26
#define GPIO_FAN_2_IN 27

#define GPIO_TEMP_R 35
#define GPIO_TEMP_G 32
#define GPIO_TEMP_B 33
#define GPIO_TEMP_BOX 34

#define GPIO_SCL 22
#define GPIO_SDA 21

uint8_t Rval = 0;
uint8_t Gval = 0;
uint8_t Bval = 0;

uint8_t RvalActual = 0;
uint8_t GvalActual = 0;
uint8_t BvalActual = 0;

double tempFactor = 1;

uint8_t Rtemp = 0;
uint8_t Gtemp = 0;
uint8_t Btemp = 0;
uint8_t driverTemp = 0;

uint16_t fanCounter1 = 0;
uint16_t fanCounter2 = 0;
ulong fanTimer = 0;

ulong tempTimer = 0;

boolean modeArtNet = true;

AsyncWebServer webServer(80);
DNSServer dnsServer;

ArtnetWifi artnet;
const int startUniverse = 0; // CHANGE FOR YOUR SETUP most software this is 1, some software send out artnet first universe as 0.

// Check if we got all universes
const int maxUniverses = 1;
bool universesReceived[maxUniverses];
bool sendFrame = 1;
int previousDataLength = 0;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>G-Spot Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js"></script>
  </head><body>
  <h2>Farben</h2>
  <form action="/color" method="post">
  <label for="color">Select color:</label>
  <input type="color" id="color" name="color" value="#%color%">
  </form><hr>
  <form action="/text" method="post">
  <label for="t">Text to display:</label>
  <textarea name="t" rows="3" cols="30"></textarea>
  <input type="submit" value="Speichern">
  </form><hr>
  <form action="/mode" method="post">
  <label for="mode">Mode:</label>
  <select name="mode" id="mode">
    <option value="art-net" %art-net-selected%>Art-Net</option>
    <option value="dmx" %dmx-selected%>DMX</option>
  </select>
  <input type="submit" value="Speichern">
  </form><hr>
</body>
<script>
    $(document).ready(function(e) {
        $("[name='color']").on('change', function() {
            var url = "/color";
            $.ajax({
                type: "POST",
                url: url,
                data: $("#color").serialize()
            });
        });
    });
</script></html>)rawliteral";

// Replaces placeholder in HTML
String processor(const String &var) {
  if (var == "color") {
    char dataString[7] = {0};
    sprintf(dataString, "%02X%02X%02X", Rval, Gval, Bval);
    return String(dataString);
  }
  if (var == "art-net-selected" && modeArtNet) {
    return "selected";
  }
  if (var == "dmx-selected" && !modeArtNet) {
    return "selected";
  }
  return String();
}

uint8_t hexStrToInt(const char str[]) {
  return (uint8_t)strtol(str, 0, 16);
}

void writeToPwm() {
  if (tempFactor == 1 && Rval == RvalActual && Gval == GvalActual && Bval == BvalActual) {
    return;
  }
  RvalActual = tempFactor * Rval;
  GvalActual = tempFactor * Gval;
  BvalActual = tempFactor * Bval;
  ledcWrite(PWM_CHANNEL_R, 255 - RvalActual);
  ledcWrite(PWM_CHANNEL_G, 255 - GvalActual);
  ledcWrite(PWM_CHANNEL_B, 255 - BvalActual);
  displayRGBIntensity(RvalActual, GvalActual, BvalActual);
}

void decodeAndUpdateColor(String colorString) {
  Rval = hexStrToInt(colorString.substring(1, 3).c_str());
  Gval = hexStrToInt(colorString.substring(3, 5).c_str());
  Bval = hexStrToInt(colorString.substring(5).c_str());
}

void onArtNetFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data) {
  if (universe == 0) {
    sendDmxFrame(length, data);
    if (length > 2) {
      Rval = data[0];
      Gval = data[1];
      Bval = data[2];
    }
  }
}

void onDmxFrame(uint16_t length, uint8_t *data) {
  // First Byte is DMX packet type, 0 is dimmer
  if (data[0] == 0 && length > 3) {
    Rval = data[1];
    Gval = data[2];
    Bval = data[3];
    memcpy(artnet.artnetPacket, data + 1, length - 1);
    artnet.write();
  }
}

void IRAM_ATTR isrFan1() {
  fanCounter1 += 1;
}

void IRAM_ATTR isrFan2() {
  fanCounter2 += 1;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(200);

  ledcSetup(PWM_CHANNEL_R, 1000, 8);
  ledcAttachPin(GPIO_LED_R, PWM_CHANNEL_R);
  ledcWrite(PWM_CHANNEL_R, 255);

  ledcSetup(PWM_CHANNEL_G, 1000, 8);
  ledcAttachPin(GPIO_LED_G, PWM_CHANNEL_G);
  ledcWrite(PWM_CHANNEL_G, 255);

  ledcSetup(PWM_CHANNEL_B, 1000, 8);
  ledcAttachPin(GPIO_LED_B, PWM_CHANNEL_B);
  ledcWrite(PWM_CHANNEL_B, 255);

  pinMode(GPIO_CASE_FAN, OUTPUT);
  digitalWrite(GPIO_CASE_FAN, LOW);

  ledcSetup(PWM_CHANNEL_FAN_OUT, 1000, 8);
  ledcAttachPin(GPIO_FAN_OUT, PWM_CHANNEL_FAN_OUT);

  pinMode(GPIO_FAN_1_IN, INPUT_PULLUP);
  attachInterrupt(GPIO_FAN_1_IN, isrFan1, FALLING);

  pinMode(GPIO_FAN_2_IN, INPUT_PULLUP);
  attachInterrupt(GPIO_FAN_2_IN, isrFan2, FALLING);

  setupDisplay();

  Serial.print("\nStarting Async_AutoConnect_ESP8266_minimal on " + String(ARDUINO_BOARD));
  Serial.println(ESP_ASYNC_WIFIMANAGER_VERSION);
  ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, "AutoConnectAP");
  ESPAsync_wifiManager.autoConnect("AutoConnectAP");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Connected. Local IP: "));
    Serial.println(WiFi.localIP());
    displayWiFi(WiFi.SSID(), WiFi.localIP());
  } else {
    Serial.println(ESPAsync_wifiManager.getStatus(WiFi.status()));
  }

  setupOta();

  artnet.begin();
  artnet.setArtDmxCallback(onArtNetFrame);
  setupDmxDefaultTx(onDmxFrame);
  displayInfo("Mode Art-Net -> DMX");

  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  webServer.on("/color", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("color", true)) {
      String value = request->getParam("color", true)->value();
      decodeAndUpdateColor(value);
    }
    request->send(200);
  });

  webServer.on("/text", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("t", true)) {
      String value = request->getParam("t", true)->value();
      displayInfo(value);
    }
    request->redirect("/");
  });

  webServer.on("/mode", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode", true)) {
      String value = request->getParam("mode", true)->value();
      if (value.equals("art-net")) {
        artnet.begin();
        dmxSwitchToTx();
        displayInfo("Mode Art-Net -> DMX");
        modeArtNet = true;
      } else if (value.equals("dmx")) {
        dmxSwitchToRx();
        displayInfo("Mode DMX -> Art-Net");
        modeArtNet = false;
      }
    }
    request->redirect("/");
  });

  webServer.begin();
}

uint8_t tempRead(uint8_t pin) {
  uint32_t adc = 0;
  for (int i = 0; i < 100; i++) {
    adc += analogRead(pin);
  }
  adc /= 100;
  double voltage = ((3.3 / 4096) * adc) + 0.155;
  double resistance = ((3.3 - voltage) * 10000) / voltage;
  uint8_t temp = round((1 / ((log(resistance / 10000) / 3977) + (1 / (25 + 273.15)))) - 273.15);
  return temp;
}

void setFanSpeed(uint8_t percent) {
  percent = min((uint8_t)100, percent);
  percent = max((uint8_t)0, percent);
  ledcWrite(PWM_CHANNEL_FAN_OUT, 255 - (percent * 2.55));
}

void tempToFanSpeed(uint8_t maxTemp) {
  if (maxTemp > 30) {
    setFanSpeed((maxTemp - 30) * 5);
  } else {
    setFanSpeed(0);
  }
}

// ToDo: Fix overshoot
void tempToTempFactor(uint8_t maxTemp) {
  if (maxTemp > 50) {
    tempFactor = 1.0 - ((double)(min((uint8_t)70, maxTemp) - 50) / 20);
  } else {
    tempFactor = 1.0;
  }
}

void handleTemp() {
  ulong timeframe = millis() - tempTimer;
  if (timeframe > 1234) {
    uint8_t tR = tempRead(GPIO_TEMP_R);
    uint8_t tG = tempRead(GPIO_TEMP_G);
    uint8_t tB = tempRead(GPIO_TEMP_B);
    uint8_t tCase = tempRead(GPIO_TEMP_BOX);
    if (tCase > 35) {
      digitalWrite(GPIO_CASE_FAN, HIGH);
    } else {
      digitalWrite(GPIO_CASE_FAN, LOW);
    }
    uint8_t maxLedTemp = max(tR, max(tG, tB));
    tempToFanSpeed(maxLedTemp);
    tempToTempFactor(maxLedTemp);
    displayTemp(tR, tG, tB, tCase);
    tempTimer = millis();
  }
}

void readFanSpeed() {
  ulong timeframe = millis() - fanTimer;
  if (timeframe > 999) {
    uint16_t fanSpeed1 = ((double)fanCounter1 / timeframe) * 30000;
    uint16_t fanSpeed2 = ((double)fanCounter2 / timeframe) * 30000;
    displayFanSpeed(fanSpeed1, fanSpeed2);
    fanCounter1 = 0;
    fanCounter2 = 0;
    fanTimer = millis();
  }
}

void loop() {
  ArduinoOTA.handle();

  handleTemp();
  readFanSpeed();

  if (modeArtNet) {
    artnet.read();
  } else {
    handleDmxRx();
  }
  writeToPwm();
  updateDisplay();
}