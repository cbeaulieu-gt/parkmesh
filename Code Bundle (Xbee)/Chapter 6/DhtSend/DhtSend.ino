// Running at 8Mhz and 3.3V
// Copyright 2015, Matthijs Kooijman <matthijs@stdin.nl>
//
// Permission is hereby granted, free of charge, to anyone
// obtaining a copy of this document and accompanying files, to do
// whatever they want with them without any restriction, including, but
// not limited to, copying, modification and redistribution.
//
// NO WARRANTY OF ANY KIND IS PROVIDED.
//
// This example reads values from a DHT22 sensor every 300 seconds and
// sends them to the coordinator XBee, to be read by Coordinator.ino. In
// between readings, the Arduino and XBee module are in sleep mode and
// the DHT sensor is powered off.

#include <XBee.h>
#include <AltSoftSerial.h>
#include <DHT.h>
#include <Adafruit_SleepyDog.h>
#include "binary.h"

const uint8_t DHT_DATA_PIN = 4;
const uint8_t DHT_POWER_PIN = 5;
const uint8_t XBEE_CTS_PIN = 6;
const uint8_t XBEE_SLEEPRQ_PIN = 7;

// Sensor type is DHT22, connected to pin D4.
DHT dht(DHT_DATA_PIN, DHT22);

AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial

XBeeWithCallbacks xbee;

void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  delay(1);

  // Configure pins modes
  pinMode(DHT_POWER_PIN, OUTPUT);
  pinMode(XBEE_SLEEPRQ_PIN, INPUT);
  pinMode(XBEE_CTS_PIN, INPUT);

  // Setup DHT sensor (works even when powered off)
  dht.begin();
}

uint8_t sendPacket(float temp, float humid) {
  // Prepare the Zigbee Transmit Request API packet
  ZBTxRequest txRequest;
  txRequest.setAddress64(0x0000000000000000);

  // Allocate 9 payload bytes: 1 type byte plus two floats of 4 bytes each
  AllocBuffer<9> packet;

  // Packet type, temperature, humidity
  packet.append<uint8_t>(1);
  packet.append<float>(temp);
  packet.append<float>(humid);
  txRequest.setPayload(packet.head, packet.len());

  // And send it
  uint8_t status = xbee.sendAndWait(txRequest, 5000);
  if (status == 0) {
    DebugSerial.println(F("Succesfully sent packet"));
  } else {
    DebugSerial.print(F("Failed to send packet: 0x"));
    DebugSerial.println(status, HEX);
  }
  return status;
}

// This sleeps in powerdown mode for about the specified number of ms.
// If an interrupt occurs during sleep, it will be handled and sleep
// will continue (but up to 8 seconds of sleep might be "lost").
void doSleep(uint32_t time) {
  DebugSerial.flush();

  while (time > 0) {
    // This sleeps for at most time ms, or the minimum sleep time if
    // that's more than time ms. Only some sleep times are available, so
    // multiple sleeps might be needed.
    int slept;
    if (time < 8000)
      slept = Watchdog.sleep(time);
    else
      slept = Watchdog.sleep(8000);

    if (slept >= time)
      return;
    time -= slept;
  }
}

// Wait for the given pin to become the given value. Returns true when
// that happened, or false when timeout ms passed
bool waitForPin(uint8_t pin, uint8_t value, uint16_t timeout) {
  unsigned long start = millis();
  while(true) {
    if (digitalRead(pin) == value)
      return true;
    if (millis() - start > timeout)
      return false;
  }
}

void loop() {
  // Enable DHT power and give it one second to power up, sleep while we wait.
  digitalWrite(DHT_POWER_PIN, HIGH);
  doSleep(1000);

  // Read data from the DHT sensor.
  // Force taking a new reading first. Since millis() isn't ticking
  // during sleep, without this, the DHT library will return data from
  // before the sleep, thinking it was read only a few ms ago.  Then
  // send the packet.
  dht.read(/* force */ true);
  float temp = dht.readTemperature();
  float humid = dht.readHumidity();

  // Put the DHT back to sleep
  digitalWrite(DHT_POWER_PIN, LOW);

  // Wake up the XBee module, and wait until it is awake (up to 1000ms)
  pinMode(XBEE_SLEEPRQ_PIN, OUTPUT);
  if (!waitForPin(XBEE_CTS_PIN, LOW, 1000))
    DebugSerial.println(F("XBee failed to wake up"));
  uint8_t status = sendPacket(temp, humid);
  if (status == NOT_JOINED_TO_NETWORK) {
    DebugSerial.println(F("Not joined, keeping XBee awake to join"));
    doSleep(30000);
  }

  // Put the XBee back to sleep.
  //  Don't write HIGH, but let the internal pullup take care of that
  //  (this makes this code work for 5V Arduinos as well).
  pinMode(XBEE_SLEEPRQ_PIN, INPUT);

  // Sleep for around 5 minutes
  doSleep(300000);
}
