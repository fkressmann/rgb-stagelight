#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsync_WiFiManager.h>
#include <ArtnetWifi.h>
#include "ota.h"
#include "display.h"

#define GPIO_LED_R 12
#define PWM_CHANNEL_R 0
#define GPIO_LED_G 13
#define PWM_CHANNEL_G 1
#define GPIO_LED_B 14
#define PWM_CHANNEL_B 2

#define GPIO_FAN_OUT 27
#define PWM_CHANNEL_FAN_OUT 3
#define GPIO_FAN_1_IN 25
#define GPIO_FAN_2_IN 26

#define GPIO_TEMP_R 32
#define GPIO_TEMP_G 33
#define GPIO_TEMP_B 34
#define GPIO_TEMP_BOX 35

#define GPIO_SCL 22
#define GPIO_SDA 21

uint8_t Rval = 0;
uint8_t Gval = 0;
uint8_t Bval = 0;

uint8_t RvalActual = 0;
uint8_t GvalActual = 0;
uint8_t BvalActual = 0;

float tempFactor = 1;

uint8_t Rtemp = 0;
uint8_t Gtemp = 0;
uint8_t Btemp = 0;
uint8_t driverTemp = 0;

uint16_t fanCounter1 = 0;
uint16_t fanCounter2 = 0;
ulong fanTimer = 0;

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
  <input type="color" id="color" name="color" value="#%color%"><br>
  </form>
  <form action="/text" method="post">
  <label for="t">Text to display:</label>
  <textarea name="t" rows="3" cols="30"></textarea>
  <input type="submit" value="Speichern">
  </form>
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
String processor(const String &var)
{
  if (var == "color")
  {
    char dataString[7] = {0};
    sprintf(dataString, "%02X%02X%02X", Rval, Gval, Bval);
    return String(dataString);
  }
  return String();
}

uint8_t hexStrToInt(const char str[])
{
  return (uint8_t)strtol(str, 0, 16);
}

void writeToPwm(uint8_t r, uint8_t g, uint8_t b)
{
  // if (Rval == r && Gval == g && Bval == b) {
  //   return;
  // }
  Rval = tempFactor * r;
  Bval = tempFactor * g;
  Gval = tempFactor * b;
  ledcWrite(PWM_CHANNEL_R, Rval);
  ledcWrite(PWM_CHANNEL_G, Gval);
  ledcWrite(PWM_CHANNEL_B, Bval);
  displayRGBIntensity(Rval, Gval, Bval);
}

void writeToPwm()
{
  if (tempFactor == 1 && Rval == RvalActual && Gval == GvalActual && Bval == BvalActual)
  {
    return;
  }
  RvalActual = tempFactor * Rval;
  GvalActual = tempFactor * Gval;
  BvalActual = tempFactor * Bval;
  Serial.println(tempFactor);
  ledcWrite(PWM_CHANNEL_R, RvalActual);
  ledcWrite(PWM_CHANNEL_G, GvalActual);
  ledcWrite(PWM_CHANNEL_B, BvalActual);
  displayRGBIntensity(RvalActual, GvalActual, BvalActual);
}

void decodeAndUpdateColor(String colorString)
{
  Rval = hexStrToInt(colorString.substring(1, 3).c_str());
  Gval = hexStrToInt(colorString.substring(3, 5).c_str());
  Bval = hexStrToInt(colorString.substring(5).c_str());
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data)
{
  if (universe == 0)
  {
    if (length > 2)
    {
      Rval = data[0];
      Gval = data[1];
      Bval = data[2];
    }
  }
}

void IRAM_ATTR isrFan1()
{
  fanCounter1 += 1;
}

void IRAM_ATTR isrFan2()
{
  fanCounter2 += 1;
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(200);

  ledcSetup(PWM_CHANNEL_R, 1000, 8);
  ledcAttachPin(GPIO_LED_R, PWM_CHANNEL_R);

  ledcSetup(PWM_CHANNEL_G, 1000, 8);
  ledcAttachPin(GPIO_LED_G, PWM_CHANNEL_G);

  ledcSetup(PWM_CHANNEL_B, 1000, 8);
  ledcAttachPin(GPIO_LED_B, PWM_CHANNEL_B);

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
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print(F("Connected. Local IP: "));
    Serial.println(WiFi.localIP());
    displayWiFi(WiFi.SSID(), WiFi.localIP());
  }
  else
  {
    Serial.println(ESPAsync_wifiManager.getStatus(WiFi.status()));
  }

  setupOta();

  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);

  // Send web page to client
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send_P(200, "text/html", index_html, processor); });
  // Receive an HTTP GET request at <ESP_IP>/get?threshold_input=<inputMessage>&enable_arm_input=<inputMessage2>
  webServer.on("/color", HTTP_POST, [](AsyncWebServerRequest *request)
               {
    // GET threshold_input value on <ESP_IP>/get?threshold_input=<inputMessage>
    Serial.println("Handling request");
    if (request->hasParam("color", true)) {
      Serial.println("Found color in request:");
      String value = request->getParam("color", true)->value();
      decodeAndUpdateColor(value);
    }
    request->send(200); });

  webServer.on("/text", HTTP_POST, [](AsyncWebServerRequest *request)
               {
    // GET threshold_input value on <ESP_IP>/get?threshold_input=<inputMessage>
    Serial.println("Handling request");
    if (request->hasParam("t", true)) {
      String value = request->getParam("t", true)->value();
      Serial.println("Received Text:");
      Serial.println(value);
      displayInfo(value);
    }
    request->redirect("/"); });

  webServer.begin();
}

uint8_t tempRead(uint8_t pin)
{
  uint16_t adc = 0;
  for (int i = 0; i < 10; i++)
  {
    adc += analogRead(pin);
  }
  adc /= 10;
  double voltage = ((3.3 / 4096) * adc) + 0.155;
  double resistance = ((3.3 - voltage) * 10000) / voltage;
  uint8_t temp = round((1 / ((log(resistance / 10000) / 3977) + (1 / (25 + 273.15)))) - 273.15);
  return temp;
}

void setFanSpeed(uint8_t percent)
{
  percent = min((uint8_t)100, percent);
  percent = max((uint8_t)0, percent);
  ledcWrite(PWM_CHANNEL_FAN_OUT, 255 - (percent * 2.55));
}

void tempToFanSpeed(uint8_t maxTemp)
{
  if (maxTemp > 30)
  {
    setFanSpeed((maxTemp - 30) * 5);
  }
  else
  {
    setFanSpeed(0);
  }
}

void tempToTempFactor(uint8_t maxTemp)
{
  if (maxTemp > 50)
  {
    tempFactor = 1 - ((float)(max((uint8_t) 20, maxTemp) - 50) / 20);
  }
  else
  {
    tempFactor = 1;
  }
}

void handleTemp()
{
  uint8_t tR = 0; // tempRead(GPIO_TEMP_R);
  uint8_t tG = 0; // tempRead(GPIO_TEMP_G);
  uint8_t tB = tempRead(GPIO_TEMP_B);
  uint8_t tD = tempRead(GPIO_TEMP_BOX);
  uint8_t maxTemp = max(tR, max(tG, tB));
  tempToFanSpeed(maxTemp);
  tempToTempFactor(maxTemp);
  displayTemp(tR, tG, tB, tD);
}

void handleFanSpeed()
{
  ulong timeframe = millis() - fanTimer;
  if (timeframe > 1000)
  {
    uint16_t fanSpeed1 = ((double)fanCounter1 / timeframe) * 30000;
    uint16_t fanSpeed2 = ((double)fanCounter2 / timeframe) * 30000;
    displayFanSpeed(fanSpeed1, fanSpeed2);
    fanCounter1 = 0;
    fanCounter2 = 0;
    fanTimer = millis();
  }
}

void loop()
{
  ArduinoOTA.handle();
  // put your main code here, to run repeatedly:
  handleTemp();
  handleFanSpeed();
  writeToPwm();
  updateDisplay();
  artnet.read();
}