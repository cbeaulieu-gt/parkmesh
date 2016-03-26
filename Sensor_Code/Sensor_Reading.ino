/*
 Reads the analog sensor value,
 Converts it to distance,
 Displays the distance in a Serial Port,
 Turns the LED ON when a car is detected.

 ****** This code is written for Arduino UNO ******
 
 created 03 Mar. 2016
 by Varol Burak Aydemir

 */

// These constants won't change.  They're used to give names
// to the pins used:
const int analogInPin = A0;  // Analog input pin that the IR sensor is attached to

int sensorValue = 0;        // value read from the IR sensor
float voltage = 0.0;        // IR sensor value in floating point arithmetic
float distance = 0.0;       // Distance of the object detected to the IR sensor.
boolean isCarParked = false;  //TRUE if a car is detected and FALSE if a car is not detected.

void setup() {
  // initialize serial communications at 9600 bps:
  Serial.begin(9600);
  pinMode(13, OUTPUT); //LED
}

void loop() {
  // read the analog in value:
  sensorValue = analogRead(analogInPin);
  // convert the int value to floating point arithmetic.
  voltage = sensorValue * (5.0 / 1023.0);
  // convert the analog input voltage to the distance based on the datasheet of GP2Y0A41SK IR sensor.
  distance = 12.9615 / voltage - 0.42;
  // Do a car detection based on simple thresholding and output the result to the LED.
  if (distance < 8.0)
  {
    isCarParked = true;
    digitalWrite(13, HIGH);
  }
  else
  {
    isCarParked = false;
    digitalWrite(13, LOW);
  }
  // print the results to the serial monitor:
  Serial.print("IR Sensor Reading in cm = ");
  Serial.println(distance);
  if (isCarParked)
  {
    Serial.println("Car is Detected!!!");
  }
  

  // wait 1 seconds before the next loop
  // for the analog-to-digital converter to settle
  // after the last reading and as a sampling time:
  delay(1000);
}
