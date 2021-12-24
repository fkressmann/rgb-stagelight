#include "display.h"
#include <Fonts/FreeSerifBold18pt7b.h>

char network[21];
char ipAddress[21];
char intensity[21];
char tempRGB[21];
char tempDriver[21];
char fanSpeed[21];
char info[21];
char info2[21];

// int intensityR;
// int intensityG;
// int intensityB;
// int tempR;
// int tempG;
// int tempB;
// int tempDriver;
// int fanSpped1;
// int fanSpeed2;
// int tempB;

boolean displayChanged = true;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setupDisplay()
{
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }
    display.clearDisplay();
    display.setFont(&FreeSerifBold18pt7b);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(8, 40);
    display.print("G-Spot");
    display.setFont();
    display.setCursor(0, 56);
    display.print("Felix Kressmann");
    display.display();

}

void updateDisplay()
{
    if (displayChanged)
    {
        display.clearDisplay();
        display.setTextSize(1);      // Normal 1:1 pixel scale
        display.setTextColor(WHITE); // Draw white text
        display.setCursor(0, 0);     // Start at top-left corner
        display.println(network);
        display.println(ipAddress);
        display.println(intensity);
        display.println(tempRGB);
        display.println(tempDriver);
        display.println(fanSpeed);
        display.println(info);
        display.println(info2);
        display.display();
        //   display.startscrollleft(0x00, 0x0F);
        displayChanged = false;
    }
}

void displayWiFi(String wifi, IPAddress ip) {
    sprintf(network, "WiFi %s", wifi.c_str());
    sprintf(ipAddress, "IP   %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    displayChanged = true;
}
void displayRGBIntensity(int r, int g, int b) {
    sprintf(intensity,"RGB  R%3d G%3d B%3d", r, g, b);
    Serial.println(intensity);
    displayChanged = true;
}
void displayTemp(int r, int g, int b, int driver) {
    sprintf(tempRGB,"Temp R%2d G%2d B%2d", r, g, b);
    sprintf(tempDriver,"Temp Driver %d", driver);
    displayChanged = true;
}
void displayFanSpeed(int fan1, int fan2) {
    sprintf(fanSpeed,"Fan %4drpm %4drpm", fan1, fan2);
    displayChanged = true;
}
void displayInfo(String in) {
    in.toCharArray(info, 21);
    displayChanged = true;
}
void displayInfo(char in[]) {
    strcpy(info, in);
    displayChanged = true;
}
void displayInfo2(String in) {
    in.toCharArray(info2, 21);
    displayChanged = true;
}
void displayInfo2(char in[]) {
    strcpy(info2, in);
    displayChanged = true;
}