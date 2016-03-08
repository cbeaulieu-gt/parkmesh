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
// This example receives packets containing temperature and humidity
// data, as sent by the DhtSend.ino sketch, and prints their contents to
// serial.

#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include "binary.h"

XBeeWithCallbacks xbee;

AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial

void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  delay(1);

  // Setup callbacks
  xbee.onPacketError(printErrorCb, (uintptr_t)(Print*)&DebugSerial);
  xbee.onResponse(printErrorCb, (uintptr_t)(Print*)&DebugSerial);
  xbee.onZBRxResponse(processRxPacket);
}

void processRxPacket(ZBRxResponse& rx, uintptr_t) {
  Buffer b(rx.getData(), rx.getDataLength());
  uint8_t type = b.remove<uint8_t>();

  if (type == 1 && b.len() == 8) {
    DebugSerial.print(F("DHT packet received from "));
    printHex(DebugSerial, rx.getRemoteAddress64());
    DebugSerial.println();
    DebugSerial.print(F("Temperature: "));
    DebugSerial.println(b.remove<float>());
    DebugSerial.print(F("Humidity: "));
    DebugSerial.println(b.remove<float>());
    return;
  }

  DebugSerial.println(F("Unknown or invalid packet"));
  printResponse(rx, DebugSerial);
}

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();
}
