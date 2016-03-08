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
// This example illustrates how to set up the xbee-arduino library and can
// be used to test connectivity with an XBee module. The module must be
// configured for API mode with escaping (AP=2).
//
// Any API frames received from the XBee module are printed and at
// startup, the "VR" AT command is sent to the module to retrieve the
// current firmware version.
//
// This sketch is a modified version of Connect.ino, to run on the
// Arduino Leonardo.
//
// Typical output looks like this:
//
//    Starting...
//    AtCommandResponse:
//      Command: VR
//      Status: 0x00
//      Value: 23 A7

#include <XBee.h>
#include <Printers.h>

XBeeWithCallbacks xbee;

#define DebugSerial Serial
#define XBeeSerial Serial1

void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  delay(1);

  // Let all responses be printed
  xbee.onResponse(printResponseCb, (uintptr_t)(Print*)&DebugSerial);

  // Send a "VR" command to retrieve firmware version
  AtCommandRequest req((uint8_t*)"VR");
  xbee.send(req);
}

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();
}
