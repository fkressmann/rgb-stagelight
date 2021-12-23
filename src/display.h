#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;

void setupDisplay();
void updateDisplay();
void displayWiFi(String wifi, IPAddress ip);
void displayRGBIntensity(int r, int g, int b);
void displayTemp(int r, int g, int b, int driver);
void displayFanSpeed(int fan1, int fan2);
void displayInfo(String info);
void displayInfo2(String info);