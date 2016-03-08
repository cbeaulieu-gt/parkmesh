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
// This example shows how the coordinator Arduino can receive sensor
// data through the XBee radio, and store that data for later processing
// and visualization.
//
// This version of the sketch sends the data over the serial connection
// to the computer, where it can be processed. It is intended to be used
// along with the accompanying store_and_plot.py.
//
// Data is sent one value per line, looking like this:
//
//   DATA:name_of_resource:value
//
// In addition, debug output is also sent over serial (which will never
// use this DATA: prefix).

#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include "binary.h"

// XBee object
XBeeWithCallbacks xbee;

AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial



void publish(const __FlashStringHelper *resource, float value) {
  DebugSerial.print(F("DATA:"));
  DebugSerial.print(resource);
  DebugSerial.print(F(":"));
  DebugSerial.println(value);
}

void processRxPacket(ZBRxResponse& rx, uintptr_t) {
  Buffer b(rx.getData(), rx.getDataLength());
  uint8_t type = b.remove<uint8_t>();
  XBeeAddress64 addr = rx.getRemoteAddress64();

  if (addr == 0x0013A20040DADEE0 && type == 1 && b.len() == 8) {
      publish(F("Livingroom/Temperature"), b.remove<float>());
      publish(F("Livingroom/Humidity"), b.remove<float>());
      return;
  }

  if (addr == 0x0013A20040E2C832 && type == 1 && b.len() == 8) {
      publish(F("Study/Temperature"), b.remove<float>());
      publish(F("Study/Humidity"), b.remove<float>());
      return;
  }

  DebugSerial.println(F("Unknown or invalid packet"));
  printResponse(rx, DebugSerial);
}


void setup() {
  // Setup debug output through USB
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  // Setup XBee through the hardware serial port
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);

  xbee.onZBRxResponse(processRxPacket);
  xbee.onPacketError(printErrorCb, (uintptr_t)(Print*)&Serial);
}

void loop() {

  // Check the serial port to see if there is a new packet available
  xbee.loop();
}
