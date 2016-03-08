// Copyright 2015, Matthijs Kooijman <matthijs@stdin.nl>
//
// Permission is hereby granted, free of charge, to anyone
// obtaining a copy of this document and accompanying files, to do
// whatever they want with them without any restriction, including, but
// not limited to, copying, modification and redistribution.
//
// NO WARRANTY OF ANY KIND IS PROVIDED.
//
//
// This example shows how the coordinator Arduino can control a heating
// system based on a setpoint temperature configured through Beebotte
// and per-room temperatures measured by other Arduinos and sent to us
// using XBee.
//
// This version of the sketch controls the heating by toggling a
// relay connected to another Arduino. Toggling happens by sending
// special message through the XBee radio to the other Arduino, which is
// expected to run the Relay.ino sketch.
//
// Make sure to modify the code below to match your list of rooms,
// corresponding XBee addresses for the sensor nodes and the the address
// of the relay node.
//
// Also, don't forget to enter the MAC address (printed on a sticker on the
// shield) in the code below.

#include <XBee.h>
#include <Printers.h>
#include <Ethernet.h>
#include <SPI.h>
#include <AltSoftSerial.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include "binary.h"
#include <math.h>

// Store the MQTT server, client ID, username, and password in flash memory.
// This is required for using the Adafruit MQTT library.
const char MQTT_SERVER[] PROGMEM    = "mqtt.beebotte.com";
const char MQTT_CLIENTID[] PROGMEM  = "";
const char MQTT_USERNAME[] PROGMEM  = "your_key_here";
const char MQTT_PASSWORD[] PROGMEM  = "";
const int MQTT_PORT = 1883;


// Enter a MAC address for your controller below. Newer Ethernet
// shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0x00, 0x00, 0x5E, 0x00, 0x53, 0x00 };

// Ethernet client object, handling a single TCP connection
EthernetClient client;

// MQTT object using the CC3000 Client object
// Note that this does not use the Adafruit_MQTTO_CC3000 object, which
// pulls in some watchdog timer dependency we don't really need. It
// should be optimized to work with the Adafruit CC3000 library, but
// in practice, the advantages seem to be negligable.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_PORT, MQTT_CLIENTID,
                          MQTT_USERNAME, MQTT_PASSWORD);

// Constants to identify different rooms (starting at 0). Keep NUM_ROOMS
// as the last value, so it automatically gets the right value.
enum {
  LIVINGROOM,
  STUDY,
  NUM_ROOMS,
};

// Last received temperatures
float temperatures[NUM_ROOMS] = {NAN, NAN};
// Current setpoint
float setpoint = NAN;
// Heating state
bool heating_state = false;
// Is any temperature changed since the last control loop?
bool inputs_changed = false;

const char HOUSE_SETPOINT[] PROGMEM = "House/Setpoint";
Adafruit_MQTT_Subscribe setpoint_subscription(&mqtt, HOUSE_SETPOINT);

// XBee object
XBeeWithCallbacks xbee;

AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial

// For MQTT debugging: Enable MQTT_DEBUG in Adafruit_MQTT.h

void publish(const __FlashStringHelper *resource, float value) {
  // Use JSON to wrap the data, so Beebotte will remember the data
  // (instead of just publishing it to whoever is currently listening).
  String data;
  data += "{\"data\": ";
  data += value;
  data += ", \"write\": true}";

  DebugSerial.print(F(" Publishing "));
  DebugSerial.print(data);
  DebugSerial.print(F( " to "));
  DebugSerial.println(resource);

  // Publish data and try to reconnect when publishing data fails
  if (!mqtt.publish(resource, data.c_str())) {
    DebugSerial.println(F("Failed to publish, trying reconnect..."));
    connect();

    if (!mqtt.publish(resource, data.c_str()))
      DebugSerial.println(F("Still failed to publish data"));
  }
}

bool decideHeatingState() {
  // Apply hysteresis
  float corrected = heating_state ? (setpoint + 0.5) : setpoint;

  for (uint8_t i = 0; i < NUM_ROOMS; i++) {
    // At least one room is cold - heating should be on
    if (temperatures[i] < corrected)
      return true;
  }
  // All rooms are warm enough - heating should be off
  return false;
}

// Control a remote Arduino with a relay, using the "Relay.ino" sketch
void switchHeating(bool state) {
  // Build the packet
  AllocBuffer<2> packet;
  packet.append<uint8_t>(2); // Packet type: Switch relay
  packet.append<uint8_t>(state ? 1 : 0); // Value

  // Send the packet to the node with the relay
  XBeeAddress64 addr(0x0013A20040E2C832);
  ZBTxRequest txRequest(addr, packet.head, packet.len());
  if (xbee.sendAndWait(txRequest, 5000) == 0) {
    heating_state = state;
    publish(F("House/Heating"), state);
  } else {
    DebugSerial.println(F("Failed to send packet to relay"));
  }
}


void processRxPacket(ZBRxResponse& rx, uintptr_t) {
  Buffer b(rx.getData(), rx.getDataLength());
  XBeeAddress64 addr = rx.getRemoteAddress64();
  uint8_t type = b.remove<uint8_t>();

  if (addr == 0x0013A20040DADEE0 && type == 1 && b.len() == 8) {
      temperatures[LIVINGROOM] = b.remove<float>();
      publish(F("Livingroom/Temperature"), temperatures[LIVINGROOM]);
      publish(F("Livingroom/Humidity"), b.remove<float>());
      inputs_changed = true;
      return;
  }
  if (addr == 0x0013A20040E2C832 && type == 1 && b.len() == 8) {
      temperatures[STUDY] = b.remove<float>();
      publish(F("Study/Temperature"), temperatures[STUDY]);
      publish(F("Study/Humidity"), b.remove<float>());
      inputs_changed = true;
      return;
  }

  DebugSerial.println(F("Unknown or invalid packet"));
  printResponse(rx, DebugSerial);
}

void halt(Print& p, const __FlashStringHelper *s) {
  p.println(s);
  while(true);
}

void connect() {
  client.stop(); // Ensure any old connection is closed
  uint8_t ret = mqtt.connect();
  if (ret == 0)
    DebugSerial.println(F("MQTT connected"));
  else
    DebugSerial.println(mqtt.connectErrorString(ret));
}

void setup() {
  // Setup debug output through USB
  DebugSerial.begin(115200);

  DebugSerial.println(F("Starting..."));

  // Setup XBee through the hardware serial port
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);

  // Set up the ethernet module, using DHCP
  if (Ethernet.begin(mac) == 0)
    halt(DebugSerial, F("Ethernet failed to init"));

  DebugSerial.println(F("Ethernet initialized"));

  mqtt.subscribe(&setpoint_subscription);
  connect();

  xbee.onZBRxResponse(processRxPacket);
  xbee.onOtherResponse(printErrorCb, (uintptr_t)(Print*)&DebugSerial);

  // Ensure that the heating is switched off at startup (safety in case
  // we were reset unexpectedly and the heating is still on).
  switchHeating(false);
}

// Convert a JSON string to float. Example input:
// {"data":25.2,"ts":1438674255995,"ispublic":false}
float getFloatValue(Adafruit_MQTT_Subscribe *subscription) {
  String str((const char*)subscription->lastread);
  const char *PREFIX = "{\"data\":";
  // Check for the prefix
  if (!str.startsWith(PREFIX)) {
    DebugSerial.print(F("Unsupported value received: "));
    DebugSerial.println(str);
    return NAN;
  }
  // Remove the prefix
  str.remove(0, strlen(PREFIX));
  // Convert the rest into float (as much as possible)
  return str.toFloat();
}

void handleSubscription(Adafruit_MQTT_Subscribe *subscription) {
  float value = getFloatValue(subscription);

  DebugSerial.print((const __FlashStringHelper*)subscription->topic);
  DebugSerial.print(F(" changed to "));
  DebugSerial.println(value);

  if (subscription == &setpoint_subscription) {
    setpoint = value;
    inputs_changed = true;
  }
}

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();

  // Keep the Ethernet DHCP lease current, if needed
  Ethernet.maintain();

  Adafruit_MQTT_Subscribe *subscription = mqtt.readSubscription(0);
  if (subscription)
    handleSubscription(subscription);

  if (inputs_changed) {
    if (decideHeatingState() != heating_state)
      switchHeating(!heating_state);
    inputs_changed = false;
  }
}
