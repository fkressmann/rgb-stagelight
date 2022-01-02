#include "display.h"
#include <Arduino.h>
#include <esp_dmx.h>

void setupDmxDefaultTx(void (*rxCallback)(uint16_t length, uint8_t *data));
void dmxSwitchToRx();
void dmxSwitchToTx();
void sendDmxFrame(uint16_t length, uint8_t *data);

void handleDmxRx();
