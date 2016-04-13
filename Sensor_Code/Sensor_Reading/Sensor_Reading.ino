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
  pinMode(12, OUTPUT);
  digitalWrite(12, HIGH);
}

void loop() {
  // read the analog in value:
  sensorValue = analogRead(analogInPin);
  // convert the int value to floating point arithmetic.
  voltage = sensorValue * (3.3 / 1023.0);
  // convert the analog input voltage to the distance based on the measurements for 3.3V and GPIO pin conditions.
  if (voltage < 2.2 || voltage > 0.75) { //This is the valid measurement range for the sensor.
    distance = 7.4845 / (voltage - 0.3336);
    if (distance < 10) {
      isCarParked = true;
      digitalWrite(13, HIGH);
    } else {
      isCarParked = false;
      digitalWrite(13, LOW);
    }
  } else { //Usually if voltage is outside of this range, then it means there is an object within 0-4cm to the distance.
    distance = 100; //invalid distance
    digitalWrite(13, HIGH);
    isCarParked = true;
  }

  // print the results to the serial monitor:
  Serial.print("IR Sensor Reading in cm = ");
  Serial.println(distance);
  if (isCarParked)
  {
    Serial.println("Car is Detected!!!");
  }
  /*
  uint8_t randomSignal = random(2);
  Serial.print(F("Generated Signal: "));
  Serial.println(randomSignal);
  */
  

  // wait 1 seconds before the next loop
  // for the analog-to-digital converter to settle
  // after the last reading and as a sampling time:
  delay(1000);
}
