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
// This example reads data from an AltSoftSerial port and prints this
// (in both hexadecimal and ASCII) to the main serial port (to be read
// on a computer). Any data received from the computer is forwarded to
// the AltSoftSerial port as-is. This is intended to dump all data
// received from an XBee module and allow sending data back to it, but
// can of course be used in other situations too.
//
// Typical output looks like this:
//   Starting...
//   7E 00 19 90 00 13 A2 00  ~.......
//   40 D8 5F 9D 00 00 02 48  @._....H
//   65 6C 6C 6F 2C 20 57 6F  ello, Wo
//   72 6C 64 21 3B           rld!;

#include <AltSoftSerial.h>


// Use some macros to allow changing the serial ports to use easily
AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial

void setup() {
  DebugSerial.begin(115200);
  XBeeSerial.begin(9600);
  DebugSerial.println(F("Starting..."));
}

void loop() {
  if (XBeeSerial.available()) {
    char data[8];
    uint8_t column;

    for (column = 0; column < sizeof(data); ++column) {
      // Wait up to 1 second for more data before writing out the line
      uint32_t start = millis();
      while (!XBeeSerial.available() && (millis() - start) < 1000) /* nothing */;
      if (!XBeeSerial.available())
        break;

      // Start of API packet, break to a new line
      // In transparent mode, this causes every ~ to start a newline,
      // but that's ok.
      if (column && XBeeSerial.peek() == 0x7E)
        break;

      // Read one byte and print it in hexadecimal. Store its value in
      // data[], or store '.' is the byte is not printable. data[] will
      // be printed later as an "ASCII" version of the data.
      uint8_t b = XBeeSerial.read();
      data[column] = isprint(b) ? b : '.';
      if (b < 0x10) DebugSerial.write('0');
      DebugSerial.print(b, HEX);
      DebugSerial.write(' ');
    }

    // Fill any missing columns with spaces to align lines
    for (uint8_t i = column; i < sizeof(data); ++i)
      Serial.print(F("   "));

    // Finalize the line by adding the raw printable data and a newline.
    DebugSerial.write(' ');
    DebugSerial.write(data, column);
    DebugSerial.println();
  }

  // Forward any data from the computer directly to the XBee module
  if (DebugSerial.available())
    XBeeSerial.write(DebugSerial.read());
}
