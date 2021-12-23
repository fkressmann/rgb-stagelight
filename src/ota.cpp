#include "ota.h"

void setupOta()
{
  ArduinoOTA.setHostname("g-spot-1");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "(sketch)";
    } else { // U_FS
      type = "(fs)";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
    displayInfo("Starting OTA " + type);
    updateDisplay(); 
    });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    displayInfo("Finished OTA"); 
    updateDisplay();
    });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char msg[21];
    sprintf(msg, "Progress: %u%%\r", (progress / (total / 100)));
    displayInfo2(msg); 
    updateDisplay();
    });

  ArduinoOTA.onError([](ota_error_t error) {
    char *msg;
    if (error == OTA_AUTH_ERROR) {
      msg = (char*)"Auth Failed";
    } else if (error == OTA_BEGIN_ERROR) {
      msg = (char*)"Begin Failed";
    } else if (error == OTA_CONNECT_ERROR) {
      msg = (char*)"Connect Failed";
    } else if (error == OTA_RECEIVE_ERROR) {
      msg = (char*)"Receive Failed";
    } else if (error == OTA_END_ERROR) {
      msg = (char*)"End Failed";
    } 
    displayInfo(msg);
    updateDisplay();
    });
  ArduinoOTA.begin();
}