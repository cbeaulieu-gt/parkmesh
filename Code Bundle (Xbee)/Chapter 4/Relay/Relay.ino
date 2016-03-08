http://www.doxygen.org/index.html// Copyright 2015, Matthijs Kooijman <matthijs@stdin.nl>
//
// Permission is hereby granted, free of charge, to anyone
// obtaining a copy of this document and accompanying files, to do
// whatever they want with them without any restriction, including, but
// not limited to, copying, modification and redistribution.
//
// NO WARRANTY OF ANY KIND IS PROVIDED.
//
//
// This example shows how an Arduino can control a directly-attached
// relay, according to commands sent to it through an XBee module. This
// sketch is intended to be controlled by an Arduino running the
// Coordinator_Relay.ino sketch.
//
// This sketch listens to two-byte XBee packets: The first byte must be
// 0x02 to indicate it is a "Switch relay" command, the second byte
// must be 0x00 to switch off or 0x01 to switch on.

#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include "binary.h"

// XBee object
XBeeWithCallbacks xbee;

AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial

// I/O pin to control the relay
const uint8_t RELAY_PIN = 4;

void processRxPacket(ZBRxResponse& rx, uintptr_t) {
  Buffer b(rx.getData(), rx.getDataLength());

  uint8_t type = b.remove<uint8_t>();
  // Packet type 2 is "Switch relay" command
  if (type == 2 && b.len() == 1) {
    uint8_t state = b.remove<uint8_t>();
    digitalWrite(RELAY_PIN, state);
    DebugSerial.print(F("New relay statC: "));
    DebugSerial.println(state);
  } else {
    DebugSerial.println(F("Unknown packet type, or invalid length"));
  }
}

void setup() {
  // Setup debug output through USB
  DebugSerial.begin(115200);

  DebugSerial.println(F("Starting..."));

  // Setup XBee through the hardware serial port
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);

  // Configure relay control pin as output
  pinMode(RELAY_PIN, OUTPUT);

  xbee.onZBRxResponse(processRxPacket);
  xbee.onPacketError(printErrorCb, (uintptr_t)(Print*)&DebugSerial);
}

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();
}
