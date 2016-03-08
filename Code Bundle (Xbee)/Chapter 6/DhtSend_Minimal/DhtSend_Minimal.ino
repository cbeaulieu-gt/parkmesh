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
//
// This version of this sketch pulls out the full bag of tricks to
// minimize power usage during sleeping and wakeup.
//
// Below, some current measurements are listed. These are made with an
// Arduino Pro Mini 3.3V/8Mhz in two different setups: with and without
// a regulator. When using the regulator, the power led has been
// removed. When not using the regulator, the solder jumper is cut to
// disconnect both led and regulator.
//
// When using the original DhtSend.ino sketch (without all this extra
// power saving applied), powering through the regulator, sleeping
// current is 260uA, running is 4.5mA. Without the regulator, sleeping
// current is 180uA.
//
// With this sketch, and still using the regulator, sleeping current
// drops to 170 uA, and running current to 3.5mA.
//
// When also removing the regulator and applying 3.3V directly, the
// sleeping current drastically drops to just 4uA.
//
// Most of the sleeping current is for the watchdog. If you do not need
// timed wakeup (only pin wakeup), the current can drop as low as 100nA.
//
// Above measurements were with the BOD disabled during sleep. At the
// time of writing, this required modifications inside the SleepyDog
// library, but when you read this, these changes have probably been
// merged.
//
// During sleep, power can be saved by:
//  - Disabling ADC with ADCSRA.
//  - Enabling BOD with sleep_bod_disable (should happen inside
//    Adafruit_SleepyDog).
//
// While running, power can be saved by:
//  - Disabling ADC with ADCSRA.
//  - Disabling clock to various parts using PPR or the
//    power_disable_foo() functions.
//  - Giving unused pins a defined value (using a pullup or setting them
//    as outputs), to prevent the input state from alternating between
//    LOW and HIGH all the time.
//  - Disabling digital input logic on analog pins using DIDR0 and DIDR1
//    (not used in this sketch).
//  - Not using the UART

#include <XBee.h>
#include <AltSoftSerial.h>
#include <DHT.h>
#include <Adafruit_SleepyDog.h>
#include "binary.h"
#include <avr/power.h>

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
  // Disable ADC to save power. Must happen before power_adc_disable()
  // If you need the ADC, you can also disable it only during sleep to
  // save a bit of power (check the datasheet for proper startup timing
  // of the ADC).
  ADCSRA &= ~(1 << ADEN);

  // Enable pullups on all pins we are not using
  for (uint8_t i = 0; i < NUM_DIGITAL_PINS; ++i) {
    switch (i) {
      default:
        pinMode(i, INPUT_PULLUP);
        break;
      case DHT_DATA_PIN:
      case DHT_POWER_PIN:
      case XBEE_SLEEPRQ_PIN:
      case XBEE_CTS_PIN:
      case LED_BUILTIN:
        break;
    }
  }
  // Set the LED pin as OUTPUT LOW, since the pullup will actually turn
  // it on dimly
  pinMode(LED_BUILTIN, OUTPUT);


  // Disable as much as possible in the PRR, power reduction register,
  // disabling the clock in all these parts.  This saves power when
  // running only, in powerdown mode the clock is globally disabled
  // anyway.
  // The list below assumes a 328p microcontroller. Other
  // microcontrollers might have different toggles available (or none at
  // all, PRR is limited to PicoPower parts).
  power_adc_disable();
  power_spi_disable();
  power_twi_disable();
  // You can also disable these if you do not need them. millis() needs
  // timer0, AltSoftSerial needs timer1.
  //power_timer0_disable();
  //power_timer1_disable();
  power_timer2_disable();
  power_usart0_disable();

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
  return status;
}

// This sleeps in powerdown mode for about the specified number of ms.
// If an interrupt occurs during sleep, it will be handled and sleep
// will continue (but up to 8 seconds of sleep might be "lost").
void doSleep(uint32_t time) {
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
  waitForPin(XBEE_CTS_PIN, LOW, 1000);
  uint8_t status = sendPacket(temp, humid);
  if (status == NOT_JOINED_TO_NETWORK) {
    doSleep(30000);
  }

  // Put the XBee back to sleep.
  //  Don't write HIGH, but let the internal pullup take care of that
  //  (this makes this code work for 5V Arduinos as well).
  pinMode(XBEE_SLEEPRQ_PIN, INPUT);

  // Sleep for around 5 minutes
  doSleep(300000);
}
