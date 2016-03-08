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
// This example reads values from a DHT22 sensor, and prints them to
// serial every 10 seconds.

#include <DHT.h>

// Sensor type is DHT22, connected to pin D4.
DHT dht(4, DHT22);

#define DebugSerial Serial

void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  // Setup DHT sensor
  dht.begin();
}

void loop() {
  DebugSerial.print(F("Temperature: "));
  DebugSerial.println(dht.readTemperature());
  DebugSerial.print(F("Humidity: "));
  DebugSerial.println(dht.readHumidity());
  delay(10000);
}
