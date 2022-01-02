#include "dmx.h"

int transmitPin = 17;
int receivePin = 16;
int enablePin = 21;

boolean rxActive = false;
boolean txActive = false;
boolean initialized = false;

dmx_port_t dmxPort = 2;

byte data[DMX_MAX_PACKET_SIZE];

QueueHandle_t queue;

unsigned int timer = 0;
bool dmxIsConnected = false;

void (*dmxRxCallback)(uint16_t length, uint8_t *data);

void setupDmx() {
}

void setupDmxDefaultTx(void (*rxCallback)(uint16_t length, uint8_t *data)) {
  dmx_config_t dmxConfig = DMX_DEFAULT_CONFIG;
  dmx_param_config(dmxPort, &dmxConfig);
  dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

  int queueSize = 1;
  int interruptPriority = 1;
  dmx_driver_install(dmxPort, DMX_MAX_PACKET_SIZE, queueSize, &queue,
                     interruptPriority);
  txActive = true;
  dmx_set_mode(dmxPort, DMX_MODE_TX);
  dmxRxCallback = rxCallback;
  rxActive = false;
}

void dmxSwitchToRx() {
  txActive = false;
  dmx_set_mode(dmxPort, DMX_MODE_RX);
  rxActive = true;
}

void dmxSwitchToTx() {
  rxActive = false;
  dmx_set_mode(dmxPort, DMX_MODE_TX);
  txActive = true;
}

void handleDmxRxInternal() {
  dmx_event_t packet;

  if (xQueueReceive(queue, &packet, 0)) {
    /* If this code gets called, it means we've recieved DMX data! */

    /* We should check to make sure that there weren't any DMX errors. */
    if (packet.status == DMX_OK) {

      /* If this is the first DMX data we've received, lets log it! */
      if (!dmxIsConnected) {
        Serial.println("DMX connected!");
        displayInfo2("DMX connected");
        dmxIsConnected = true;
      }

      /* We can read the DMX data into our buffer and increment our timer. */
      dmx_read_packet(dmxPort, data, packet.size);
      timer += packet.duration;

      dmxRxCallback(packet.size, data + 1);

      /* Print a log message every 1 second (1,000,000 microseconds). */
      if (timer >= 1000000) {
        /* Print the received start code - it's usually 0.*/
        Serial.printf("Start code is 0x%02X and slot 1 is 0x%02X\n",
                      data[0], data[1]);
        timer -= 1000000;
      }
    } else {
      /* Uh-oh! Something went wrong receiving DMX. */
      Serial.println("DMX error!");
      displayInfo2("DMX error!");
    }
  } else {
    // Serial.println("No DMX package available");
  }
}

void handleDmxRx() {
  if (rxActive) {
    handleDmxRxInternal();
  }
}

void sendDmxFrame(uint16_t length, uint8_t *data) {
  // Crop frames to remove trailing 0 data
  // for (int i = length - 1; i >= 0; i--) {
  //   if (data[i]) {
  //     length = i + 1;
  //     break;
  //   }
  // }
  dmx_write_packet(dmxPort, data, length);
  // Wait for bus to be free before attempting to send data to avoid collisisons
  dmx_wait_tx_done(dmxPort, DMX_TX_PACKET_TOUT_TICK);
  dmx_tx_packet(dmxPort);
}