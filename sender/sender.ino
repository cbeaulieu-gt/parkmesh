#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>

// DONT CHANGE THESE DECLARATIONS++++++++++++++++++

AltSoftSerial SoftSerial;
XBeeWithCallbacks xbee;
#define DebugSerial Serial
#define XBeeSerial SoftSerial
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error This code relies on little endian integers!
#endif

//+++++++++++++++++++++++++++++++++++++++++++++++++

//Node Class

class Node {
  public:
    uint32_t serialNumberHigh;
    uint32_t serialNumberLow;
    uint8_t nodeIdentifier;
};

//List of AT Commands+++++++++++++++++++++++++

uint8_t FN[] = { 'F', 'N' }; //Find Network Devices
uint8_t ND[] = { 'N', 'D' }; //Network Discovery
uint8_t NO[] = { 'N', 'O' }; //Network Discovery Options
uint8_t SH[] = { 'S', 'H' }; //Serial High
uint8_t SL[] = { 'S', 'L' }; //Serial Low
uint8_t SM[] = { 'S', 'M' }; //Serial Low

//List of Internal Commands
uint8_t setBackupOrigin = 0x80;
uint8_t reinitializeNetwork = 0x90;

//Network Variables++++++++++++++++++++++++++++++++

//These relate to other nodes in the network. Put anything here
//that is not specific to this particular node or would need to be set
//for all nodes in the network.

const int MAX_NODES = 10;
Node AllNodes[MAX_NODES];
Node DestinationNodes[3];
unsigned long cycleDuration = 10000;
unsigned long waitTime = 10000;
unsigned long deviceInitializationTime = 30000;
uint8_t dataPacket[MAX_NODES];

//Local Node+++++++++++++++++++++++++++++++++++++++

//Specfic only to this node. Operation specfic variables should be
//placed here.

///// Sensor Integration Code /////
/*const int analogInPin = A0;  // Analog input pin that the IR sensor is attached to
int sensorValue = 0;        // value read from the IR sensor
float voltage = 0.0;        // IR sensor value in floating point arithmetic
float distance = 0.0;       // Distance of the object detected to the IR sensor.
boolean isCarParked = false;*/  //TRUE if a car is detected and FALSE if a car is not detected.
///// Sensor Integration Code End /////

Node LocalNode;
int transmitDataIndex = 0;
int neighbors = 0;
int totalNodes = 0;
unsigned long cycleStartTime = 0;
bool deviceSetupComplete = false;
bool packetProcessed = false;
int cycleMissCount = 0;

// For now we are counting down towards the hub. This means that the origin
// node will set itself if it does not hear from any nodes with a higher
//NI than itself.
bool originNode = false;
bool backupOriginNode = false;
int originMissCount = 0;
char chTemp[3];

//For the hub only
bool isHub = false;

//Neede to sleep Xbee on the case of node failure.
const uint8_t XBEE_SLEEPRQ_PIN = 7;

//=================================================================================================
//=================================================================================================


//Determines if this node is the origin node, i.e. the furthest node away from the hub
//and the node that will be the first to transmit data.
bool IsOriginNode()
{
  DebugSerial.println(F("Checking to see if this is the origin node."));

  //Because the array is sorted by NIs and the largest NI is the origin node, we can simply
  //check if the NI in the topmost indices matches ours, and if so that must mean we are the
  //the origin(assuming no identical NIs).
  if (LocalNode.nodeIdentifier == AllNodes[0].nodeIdentifier)
  {
    originNode = true;
    DebugSerial.println(F("This is the ORIGIN node."));
    DebugSerial.println(F(""));
  }
  else
  {
    if (LocalNode.nodeIdentifier == AllNodes[1].nodeIdentifier)
    {
      backupOriginNode = true;
      DebugSerial.println(F("This is the BACKUP ORIGIN node."));
      DebugSerial.println(F(""));
    }
    else
    {
      DebugSerial.println(F("This is NOT the origin node."));
      DebugSerial.println(F(""));
    }
  }

  return true;
}

bool IsHubNode()
{
  DebugSerial.println(F("Checking to see if this is the hub node."));

  uint8_t currentMinimum = -1;
  for (int index = 0; index < totalNodes; index++)
  {
    //DebugSerial.println(AllNodes[index].nodeIdentifier);
    if (AllNodes[index].nodeIdentifier < currentMinimum)
    {
      currentMinimum = AllNodes[index].nodeIdentifier;

    }
  }
  SendNodeIdentifiers();
  if (currentMinimum == LocalNode.nodeIdentifier)
  {
    isHub = true;
    //DebugSerial.println("Setting the Hub");
    SendNodeIdentifiers();
  }

  return true;
}

//Forward all node identifiers to the Raspberry PI
void SendNodeIdentifiers()
{
  DebugSerial.println("");
  DebugSerial.println("Sending Node Identifiers:");
  for (int i = 0; i < sizeof(AllNodes) / sizeof(AllNodes[0]); i++)
  {
    Serial.println(AllNodes[i].nodeIdentifier);
  }
}

//Sends a  message to a node in the network informing it that it is the new
//backup origin for the network.
bool CommandNewBackupOrigin()
{
  DebugSerial.println(F("Informing next node of origin."));

  //Recieving nodes will listen for this code and upon getting it will store this node identifer
  //as the origin node
  uint8_t data[] = { setBackupOrigin };

  Node node = Node();
  node.nodeIdentifier = DestinationNodes[0].nodeIdentifier;
  node.serialNumberHigh = DestinationNodes[0].serialNumberHigh;
  node.serialNumberLow = DestinationNodes[0].serialNumberLow;

  bool announcementSuccessful = SendPacket(node, data, sizeof(data));
  if (announcementSuccessful)
  {
    DebugSerial.println(F("Announcement succesful."));
    return true;
  }
  else
  {
    DebugSerial.println(F("Announcement failed."));
    return false;
  }
}

//Retrieves the Serial Number(high and low) of the node and stores it in the local node variable.
bool GetLocalNodeInformation()
{
  //If both of these become true the function was succesful and can return
  //true at the end.
  bool gotSerialLow = false;
  bool gotSerialHigh = false;

  AtCommandRequest atCommandRequest = AtCommandRequest(SH);
  AtCommandResponse atCommandResponse = AtCommandResponse();

  //Send Serial High Command Command
  xbee.send(atCommandRequest);
  DebugSerial.println(F("SerialHigh(SH) command sent to Xbee."));

  while (xbee.readPacket(500))
  {
    //Check to see if the command we got a response for matches what was expected.
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
    {
      xbee.getResponse().getAtCommandResponse(atCommandResponse);

      //Check status of response.
      DebugSerial.print("Checking command response type:");
      DebugSerial.println(atCommandResponse.getStatus(), HEX);
      if (atCommandResponse.isOk())
      {

        DebugSerial.println(F("Got command response: Successful."));
        if (atCommandResponse.getValueLength() > 0)
        {
          int commandLength = atCommandResponse.getValueLength();
          DebugSerial.print(F("Command value length: "));
          DebugSerial.println(commandLength, DEC);

          //Print the entire command in HEX (for debugging).
          DebugSerial.println(F("Command value: "));
          for (int index = 0; index < commandLength; index++)
          {
            uint8_t commandValue = atCommandResponse.getValue()[index];
            DebugSerial.print(commandValue, HEX);
          }

          uint32_t serialHigh = 0;
          for (int index = 0; index < commandLength; index++)
          {
            serialHigh = (serialHigh << 8) + atCommandResponse.getValue()[index];
          }

          //Store the value into the local node.
          LocalNode.serialNumberHigh = serialHigh;
          DebugSerial.println("");
          gotSerialHigh = true;
        }
        DebugSerial.println(F(""));
      }
      else
      {
        DebugSerial.println(F("The command response does not match expected command."));
      }
    }
    else
    {
      DebugSerial.println(F("The response was not succesful."));
    }
  }


  //Repeat the process to get the SerialLow
  atCommandRequest = AtCommandRequest(SL);
  atCommandResponse = AtCommandResponse();

  //Send Serial Low Command Command
  xbee.send(atCommandRequest);
  DebugSerial.println(F("SerialLow(SL) command sent to Xbee."));

  while (xbee.readPacket(500))
  {
    //Check to see if the command we got a response for matches what was expected.
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
    {
      xbee.getResponse().getAtCommandResponse(atCommandResponse);

      //Check status of response.
      DebugSerial.print("Checking command response type:");
      DebugSerial.println(atCommandResponse.getStatus(), HEX);
      if (atCommandResponse.isOk())
      {

        DebugSerial.println(F("Got command response: Successful."));
        if (atCommandResponse.getValueLength() > 0)
        {
          int commandLength = atCommandResponse.getValueLength();
          DebugSerial.print(F("Command value length: "));
          DebugSerial.println(commandLength, DEC);

          //Print the entire command in HEX (for debugging).
          DebugSerial.println(F("Command value: "));
          for (int index = 0; index < commandLength; index++)
          {
            uint8_t commandValue = atCommandResponse.getValue()[index];
            DebugSerial.print(commandValue, HEX);
          }

          uint32_t serialLow = 0;
          for (int index = 0; index < commandLength; index++)
          {
            serialLow = (serialLow << 8) + atCommandResponse.getValue()[index];
          }

          //Store the value into the local node.
          LocalNode.serialNumberLow = serialLow;
          DebugSerial.println("");
          gotSerialLow = true;
        }
        DebugSerial.println(F(""));

      }
      else
      {
        DebugSerial.println(F("The command response does not match expected command."));
      }
    }
    else
    {
      DebugSerial.println(F("The response was not succesful."));
    }
  }

  //Both need to work in order for the function to return true.
  if (gotSerialHigh && gotSerialLow)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool SendATCommand(uint8_t command[], uint8_t commandValue)
{
  AtCommandRequest atCommandRequest = AtCommandRequest(command);
  AtCommandResponse atCommandResponse = AtCommandResponse();
  atCommandRequest.setCommandValue(&commandValue);
  atCommandRequest.setCommandValueLength(sizeof(uint8_t));
  xbee.send(atCommandRequest);

  while (xbee.readPacket(500))
  {
    //Check to see if the command we got a response for matches what was expected.
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
    {
      xbee.getResponse().getAtCommandResponse(atCommandResponse);

      //Check status of response.
      DebugSerial.print("Checking command response type:");
      DebugSerial.println(atCommandResponse.getStatus(), HEX);
      if (atCommandResponse.isOk())
      {

        DebugSerial.println(F("Got command response: Successful."));
        if (atCommandResponse.getValueLength() > 0)
        {
          int commandLength = atCommandResponse.getValueLength();
          DebugSerial.print(F("Command value length: "));
          DebugSerial.println(commandLength, DEC);

          //Print the entire command in HEX (for debugging).
          DebugSerial.println(F("Command value: "));
          for (int index = 0; index < commandLength; index++)
          {
            uint8_t commandValue = atCommandResponse.getValue()[index];
            DebugSerial.print(commandValue, HEX);
          }
          DebugSerial.println("");
        }
        DebugSerial.println(F(""));
        return true;
      }
      else
      {
        DebugSerial.println(F("The command response does not match expected command."));
      }
    }
    else
    {
      DebugSerial.println(F("The response was not succesful."));
    }
  }

  return false;
}

//Prints the array of nodes found in the FindNeighbor(FN) command.
void PrintNodeArray()
{
  for (Node node : AllNodes)
  {
    //Since the whole array is pre-allocated we only want to display
    //entries that actually contain information.
    if (node.serialNumberHigh != 0)
    {
      DebugSerial.print(F("SH: "));
      DebugSerial.print(node.serialNumberHigh, HEX);
      DebugSerial.print(F(";  SL: "));
      DebugSerial.print(node.serialNumberLow, HEX);
      DebugSerial.print(F(";  NI: "));
      DebugSerial.println(node.nodeIdentifier, DEC);
    }
  }

  DebugSerial.println("");
}

//Clears the array of found nodes.
void ClearNodeArray()
{
  for (int ii = 0; ii < MAX_NODES; ii++)
  {
    AllNodes[ii].nodeIdentifier = 0;
    AllNodes[ii].serialNumberHigh = 0;
    AllNodes[ii].serialNumberLow = 0;
  }
}

//Clears the array of found nodes.
void ClearDataArray()
{
  for (int ii = 0; ii < MAX_NODES; ii++)
  {
    dataPacket[ii] = 0;
  }
}

//Sorts the array of found nodes by the NodeIdentifiers from largest to smallest.
void SortNodeArray()
{
  //Implements a simple bubble up function. Note that MAX_NODES is static and defines the
  //size of the array so we dont need to check the length of the array directly.
  for (int ii = 0; ii < MAX_NODES; ii++)
  {
    for (int jj = ii + 1; jj < MAX_NODES; jj++)
    {
      if (AllNodes[ii].nodeIdentifier < AllNodes[jj].nodeIdentifier)
      {
        Node temp = AllNodes[ii];
        AllNodes[ii] = AllNodes[jj];
        AllNodes[jj] = temp;
      }
    }
  }
}

//Finds all immediate nieghbors of the node and stores their Serial Numbers (High and Low) an
//the Node Identifier into an array. Note that this discovers all nodes in the netowrk, so the results
//will need to be filtered later to select proper nieghbors.
bool FindNodes()
{
  bool gotResponse = false;

  AtCommandRequest atCommandRequest = AtCommandRequest(FN);
  AtCommandResponse atCommandResponse = AtCommandResponse();

  //Send Network Discovery Command
  xbee.send(atCommandRequest);
  DebugSerial.println(F("FindNeighbors(FN) command sent to Xbee."));

  int index = 0;

  //This should be long since there is a high potential for congestion during this phase of the setup.
  while (xbee.readPacket(10000))
  {
    //Check to see if the command we got a response for matches what was expected.
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
    {
      xbee.getResponse().getAtCommandResponse(atCommandResponse);

      //Check status of response.
      DebugSerial.print("Checking command response type:");
      DebugSerial.println(atCommandResponse.getStatus(), HEX);
      if (atCommandResponse.isOk())
      {

        DebugSerial.println(F("Got command response: Successful."));
        if (atCommandResponse.getValueLength() > 0)
        {
          int commandLength = atCommandResponse.getValueLength();
          DebugSerial.print(F("Command value length: "));
          DebugSerial.println(commandLength, DEC);

          //Print the entire command in HEX (for debugging).
          DebugSerial.println(F("Command value: "));
          for (int index = 0; index < commandLength; index++)
          {
            uint8_t commandValue = atCommandResponse.getValue()[index];
            DebugSerial.print(commandValue, HEX);
          }
          DebugSerial.println("");

          uint32_t sh = 0;
          uint32_t sl = 0;
          for (int i = 2; i < 6; i++)
          {
            sh = sh << 8;
            sh = sh | atCommandResponse.getValue()[i];
          }
          for (int i = 6; i < 10; i++)
          {
            sl = sl << 8;
            sl = sl | atCommandResponse.getValue()[i];
          }

          //Prepare to capture the NodeIdentifier. This requries a little extra work
          //because the location of the identifier and the length are both variable.

          uint32_t nodeIdentifier = 0;
          uint8_t val;
          int startCaptureIndex = 0;
          int endCaptureIndex = 0;
          bool startIndexFound = false;
          for (int index = 0; index < atCommandResponse.getValueLength(); index++)
          {
            //The expected format for a Node Identifer is XX-## where
            //## is the integer value of the node. The ASCII representation
            //of '-' is 0x2D. Once we have seen this character we can begin
            //to capture the identifier.
            val = atCommandResponse.getValue()[index];
            if (val == 0x2D)
            {
              startCaptureIndex = index + 1;
              startIndexFound = true;
              DebugSerial.println("Start capture index found.");
            }

            //We need the boolean because the trigger value can appear at the beginning
            //of the packet as well. We only care about the one that will appear
            //after the NI has been encountered.
            if (val == 0xFF && startIndexFound)
            {
              endCaptureIndex = index - 1;
              DebugSerial.println("End capture index found. Breaking from loop.");
              break;
            }
          }

          //We have the beginning and ending index of the identifier. We can now
          //store the values. Note that the value comes in ASCII so it needs to be
          //converted to decimal.
          for (int index = startCaptureIndex; index < endCaptureIndex; index++)
          {
            val = atCommandResponse.getValue()[index];
            nodeIdentifier = nodeIdentifier * 10 + (val - 0x30);
          }

          //If these are both equal we found the response from the local node and can store the
          //returned Node Identifier.
          if (LocalNode.serialNumberHigh == sh && LocalNode.serialNumberLow == sl)
          {
            DebugSerial.println(F("Found the Local Node Identifier."));
            DebugSerial.print(F("NI: "));
            DebugSerial.println(nodeIdentifier);
            LocalNode.nodeIdentifier = nodeIdentifier;
          }
          else
          {
            AllNodes[index].serialNumberHigh = sh;
            DebugSerial.print(F("SH: "));
            DebugSerial.println(sh, HEX);

            AllNodes[index].serialNumberLow = sl;
            DebugSerial.print(F("SL: "));
            DebugSerial.println(sl, HEX);

            AllNodes[index].nodeIdentifier = nodeIdentifier;
            DebugSerial.print(F("NI: "));
            DebugSerial.println(nodeIdentifier);

            //This is in here since we dont count a response from ourself, we only care
            //about external nodes.
            gotResponse = true;
          }

          index++;
        }

        DebugSerial.println(F(""));
      }
      else
      {
        DebugSerial.println(F("The command response does not match what was expected."));
      }
    }
    else
    {
      DebugSerial.println(F("The response was not successful."));
    }
  }

  DebugSerial.println(F("No more packet founds."));
  return gotResponse;

}

//Finds all nodes in the network and populates the AllNodes array according to the repsonses.
bool NetworkDiscovery()
{
  bool gotResponse = false;

  AtCommandRequest atCommandRequest = AtCommandRequest(ND);
  AtCommandResponse atCommandResponse = AtCommandResponse();

  //Send Network Discovery Command
  xbee.send(atCommandRequest);
  DebugSerial.println(F("NetworkDiscovery(ND) command sent to Xbee."));

  int index = 0;

  //This should be long since there is a high potential for congestion during this phase of the setup.
  while (xbee.readPacket(10000))
  {
    //Check to see if the command we got a response for matches what was expected.
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
    {
      xbee.getResponse().getAtCommandResponse(atCommandResponse);

      //Check status of response.
      DebugSerial.print("Checking command response type:");
      DebugSerial.println(atCommandResponse.getStatus(), HEX);
      if (atCommandResponse.isOk())
      {

        DebugSerial.println(F("Got command response: Successful."));
        if (atCommandResponse.getValueLength() > 0)
        {
          int commandLength = atCommandResponse.getValueLength();
          DebugSerial.print(F("Command value length: "));
          DebugSerial.println(commandLength, DEC);

          //Print the entire command in HEX (for debugging).
          DebugSerial.println(F("Command value: "));
          for (int index = 0; index < commandLength; index++)
          {
            uint8_t commandValue = atCommandResponse.getValue()[index];
            DebugSerial.print(commandValue, HEX);
          }
          DebugSerial.println("");

          uint32_t sh = 0;
          uint32_t sl = 0;
          for (int i = 2; i < 6; i++)
          {
            sh = sh << 8;
            sh = sh | atCommandResponse.getValue()[i];
          }
          for (int i = 6; i < 10; i++)
          {
            sl = sl << 8;
            sl = sl | atCommandResponse.getValue()[i];
          }

          //Prepare to capture the NodeIdentifier. This requries a little extra work
          //because the location of the identifier and the length are both variable.

          uint32_t nodeIdentifier = 0;
          uint8_t val;
          int startCaptureIndex = 0;
          int endCaptureIndex = 0;
          bool startIndexFound = false;
          for (int index = 0; index < atCommandResponse.getValueLength(); index++)
          {
            //The expected format for a Node Identifer is XX-## where
            //## is the integer value of the node. The ASCII representation
            //of '-' is 0x2D. Once we have seen this character we can begin
            //to capture the identifier.
            val = atCommandResponse.getValue()[index];
            if (val == 0x2D)
            {
              startCaptureIndex = index + 1;
              startIndexFound = true;
              DebugSerial.println("Start capture index found.");
            }

            //We need the boolean because the trigger value can appear at the beginning
            //of the packet as well. We only care about the one that will appear
            //after the NI has been encountered.
            if (val == 0xFF && startIndexFound)
            {
              endCaptureIndex = index - 1;
              DebugSerial.println("End capture index found. Breaking from loop.");
              break;
            }
          }

          //We have the beginning and ending index of the identifier. We can now
          //store the values. Note that the value comes in ASCII so it needs to be
          //converted to decimal.
          for (int index = startCaptureIndex; index < endCaptureIndex; index++)
          {
            val = atCommandResponse.getValue()[index];
            nodeIdentifier = nodeIdentifier * 10 + (val - 0x30);
          }

          //If these are both equal we found the response from the local node and can store the
          //returned Node Identifier.
          if (LocalNode.serialNumberHigh == sh && LocalNode.serialNumberLow == sl)
          {
            DebugSerial.println(F("Found the Local Node Identifier."));
            DebugSerial.print(F("NI: "));
            DebugSerial.println(nodeIdentifier);
            LocalNode.nodeIdentifier = nodeIdentifier;
          }

          AllNodes[index].serialNumberHigh = sh;
          DebugSerial.print(F("SH: "));
          DebugSerial.println(sh, HEX);

          AllNodes[index].serialNumberLow = sl;
          DebugSerial.print(F("SL: "));
          DebugSerial.println(sl, HEX);

          AllNodes[index].nodeIdentifier = nodeIdentifier;
          DebugSerial.print(F("NI: "));
          DebugSerial.println(nodeIdentifier);
          totalNodes++;

          //This is in here since we dont count a response from ourself, we only care
          //about external nodes.
          gotResponse = true;

          index++;
        }

        DebugSerial.println(F(""));
      }
      else
      {
        DebugSerial.println(F("The command response does not match what was expected."));
      }
    }
    else
    {
      DebugSerial.println(F("The response was not successful."));
    }
  }

  DebugSerial.println(F("No more packet founds."));
  return gotResponse;

}

//Extracts potential destination nodes from the list of all nodes that responded
//to the Network Discovery command.
bool SelectNeighbors()
{
  //Note that the array has been pre-sorted (biggest to smallest) so we can take the
  //first 3 entries we encounter that satisfy the conditions without worrying about
  //grabbing nodes too far down the list.
  for (int index = 0; index < MAX_NODES; index++)
  {
    if (AllNodes[index].nodeIdentifier < LocalNode.nodeIdentifier && neighbors < 3 && AllNodes[index].nodeIdentifier != 0)
    {
      DestinationNodes[neighbors] = AllNodes[index];
      DebugSerial.println(F("Neighbor selected."));
      DebugSerial.print(F("SH: "));
      DebugSerial.print(DestinationNodes[neighbors].serialNumberHigh, HEX);
      DebugSerial.print(F(";  SL: "));
      DebugSerial.print(DestinationNodes[neighbors].serialNumberLow, HEX);
      DebugSerial.print(F(";  NI: "));
      DebugSerial.println(DestinationNodes[neighbors].nodeIdentifier, DEC);

      neighbors++;
    }
  }

  //For now this is advisory but in a real network this should cause the node to either shut down
  //or re-initiate a Network Discovery at a later period.
  if (neighbors == 0)
  {
    DebugSerial.println(F("No suitable neighbors found."));
    return false;
  }
  else
  {
    return true;
  }
}

//Retrieve data from the sensor device.
uint8_t GetSensorData()
{
  //For now this function will simply get a random piece of data. Either a zero or a one.
  uint8_t randomSignal = random(2);
  DebugSerial.print(F("Generated Signal: "));
  DebugSerial.println(randomSignal);

  ///// Sensor Integration Code /////
  /*digitalWrite(12, HIGH);
  uint8_t carParked = 0;
  delay(100); // wait for sensor output to stabilize
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
      carParked = 1;
    } else {
      isCarParked = false;
      digitalWrite(13, LOW);
      carParked = 0;
    }
  } else { //Usually if voltage is outside of this range, then it means there is an object within 0-4cm to the distance.
    distance = 100; //invalid distance
    digitalWrite(13, HIGH);
    isCarParked = true;
    carParked = 1;
  }
  digitalWrite(12, LOW);
  return carParked;*/
  ///// Sensor Integration Code End /////

  return randomSignal;
}

//This function will grab the sensor data and will append it to the end of the data array.
//This array should have been previously populated by any forwarding data recieved from nodes further
//down the chain.
void AppendSensorData()
{
  uint8_t sensorData = GetSensorData();

  uint8_t payload = LocalNode.nodeIdentifier << 1 | sensorData;
  DebugSerial.print(F("Local Payload : "));
  sprintf(chTemp, "%02X,", payload);
  DebugSerial.println(chTemp);

  //There is no need to check for size as the array is predefined
  //to be able to hold an entry for every node in the network and no node will ever be
  //able to speak with all other nodes.

  dataPacket[transmitDataIndex] = payload;
  transmitDataIndex++;
}

//Sends data (both sensor and forwarding data) to the next node in the chain. Will attempt to send
//to each destination node in succession until it either succeeds or runs out of destination nodes.
bool TransmitData()
{
  int index = 0;
  bool packetSent = false;

  //Grab the data from the stream and place it into a global array
  DebugSerial.print(F("Data in data array before storage:"));

  for (int ii = 0; ii < sizeof(dataPacket) / sizeof(uint8_t); ii++)
  {
    sprintf(chTemp, "%02X,", dataPacket[ii]);
    DebugSerial.print(chTemp);
  }
  DebugSerial.println(F(""));
  DebugSerial.print(F("before making data Packet transmitDataIndex: "));
  DebugSerial.println(transmitDataIndex);
  DebugSerial.println();

  //Because of the way we are passing variables we need a temporary array to store the data we are
  //sending
  uint8_t temp[transmitDataIndex];
  for (int index = 0; index < transmitDataIndex; index++)
  {
    temp[index] = dataPacket[index];
  }

  //Grab the data from the stream and place it into a global array
  DebugSerial.print(sizeof(temp));
  DebugSerial.print(F("Data in temporary array:"));

  for (int ii = 0; ii < sizeof(temp) / sizeof(uint8_t); ii++)
  {
    sprintf(chTemp, "%02X,", temp[ii]);
    DebugSerial.print(chTemp);
  }
  DebugSerial.println(F(""));

  //This loop will attempt to send the data to the next node, whose information is located in
  //the array of selected neighbors. It will keep attempting to do so until it either succeeds in
  //transmitting to one, or it reaches the end of the list of potential recipients.
  while (packetSent == false && index < neighbors)
  {
    packetSent = SendPacket(DestinationNodes[index], temp, sizeof(temp) / sizeof(uint8_t));
    index++;
  }

  //Finally clear the array of data.
  ClearDataArray();
  return packetSent;
}

//Attempts to send a single packet. Can do either targeted or broadcast transmissions.
bool SendPacket(Node targetNode, uint8_t data[], uint8_t dataSize)
{

  //The request and the data are independent of the specific node we are trying to send to
  //so they can be declared outside of the loop.
  ZBTxRequest transmitRequest;
  transmitRequest.setPayload(data, dataSize);

  //Grab the data from the stream and place it into a global array
  DebugSerial.println(F("Data to be transmitted:"));
  for (int ii = 0; ii < dataSize; ii++)
  {
    sprintf(chTemp, "%02X,", data[ii]);
    DebugSerial.print(chTemp);
    //transmitDataIndex = ii;
  }
  DebugSerial.println(F(""));
  //These will change based on the target so they need to be declared inside of the loop.
  uint64_t address = (uint64_t)targetNode.serialNumberHigh << 32 | (uint64_t)targetNode.serialNumberLow;
  transmitRequest.setAddress64(address);

  DebugSerial.print(F("Target Information: "));
  DebugSerial.print(F("Node Identifier:"));
  DebugSerial.println(targetNode.nodeIdentifier);
  DebugSerial.print(F("Serial High:"));
  DebugSerial.println(targetNode.serialNumberHigh, HEX);
  DebugSerial.print(F("Serial Low:"));
  DebugSerial.println(targetNode.serialNumberLow, HEX);

  //For now wait 5 seconds. Might want to lower this for the full network configuration.
  uint8_t status = xbee.sendAndWait(transmitRequest, 5000);

  if (status != 0)
  {
    DebugSerial.println("Transmission Error.");
    DebugSerial.print(F("Status: 0x"));
    DebugSerial.println(status, HEX);
    return false;
  }
  else
  {
    DebugSerial.println("Transmission Success.");
    DebugSerial.print(F("Status: 0x"));
    DebugSerial.println(status, HEX);
    transmitDataIndex = 0;
    return true;
  }
}

//Waits for a packet for the specified time interval. Will process the packet, and if it is a Command packet,
//the proper actions will taken to satisfy the command.
bool WaitForPacket(unsigned long waitTime)
{
  if (RecievePacket(waitTime))
  {
    return ProcessPacket();
  }
  else
  {
    return false;
  }
}

bool RecievePacket(unsigned long waitTime)
{
  DebugSerial.println(F("Calling Recieve Packet"));
  // Wait up to 1 second for more data before writing out the line
  uint32_t start = millis();
  while (!XBeeSerial.available() && (millis() - start) < waitTime) /* nothing */;
  if (!XBeeSerial.available()) {
    DebugSerial.println(F("Nothing found. Exiting recievePacket."));
    return false;
  }

  DebugSerial.println(F("Packet found!!"));
  // Start of API packet, break to a new line
  // In transparent mode, this causes every ~ to start a newline,
  // but that's ok.
  if (XBeeSerial.peek() != 0x7E)
    return false;

  // Read one byte and print it in hexadecimal. Store its value in
  // data[], or store '.' is the byte is not printable. data[] will
  // be printed later as an "ASCII" version of the data.
  uint8_t preData = 0;
  while (preData != 0xC1)
  {
    preData = XBeeSerial.read();
    DebugSerial.print(preData, HEX);
    start = millis();
    while (!XBeeSerial.available() && (millis() - start) < 1000) /* nothing */;
  }
  DebugSerial.println("");

  DebugSerial.println(F("Found start of data. Beginning to populate forwarding array."));
  transmitDataIndex = 0;
  uint8_t byteVar;
  int counter = 0;
  while (XBeeSerial.available())
  {
    //This is to prevent the capture of the checksum.
    if (transmitDataIndex == 0)
    {
      byteVar = XBeeSerial.read();
      start = millis();
      while (!XBeeSerial.available() && (millis() - start) < 1000) /* nothing */;
    }
    sprintf(chTemp, "%02X,", byteVar);
    DebugSerial.print(chTemp);
    DebugSerial.println(transmitDataIndex);
    dataPacket[transmitDataIndex++] = byteVar;
    byteVar = XBeeSerial.read();
    start = millis();
    while (!XBeeSerial.available() && (millis() - start) < 1000) /* nothing */;
    sprintf(chTemp, "%02X,", byteVar);
    DebugSerial.print(chTemp);
    counter++;
  }
  DebugSerial.println(counter);
  DebugSerial.println(transmitDataIndex);

  DebugSerial.println(F("\nFinished reading data."));
  // Forward any data from the computer directly to the XBee module
  if (DebugSerial.available())
    XBeeSerial.write(DebugSerial.read());
  return true;
}

//Will check to see if the received packet is a command packet. If so calls the appropriate function to handle the command.
//Otherwise, no action is taken.
bool ProcessPacket()
{
  //Check to the topmost bit of the packet. If it is set to one
  //this is a command packet that needs to be processed.
  if (dataPacket[0] == 0x80)
  {
    DebugSerial.println("Command Packet recieved. Processing command now.");
    return ProcessCommandPacket();
  }
  else
  {
    DebugSerial.println("Data packet recieved. No processing necessary.");
    return true;
  }
}

bool ProcessCommandPacket()
{
  bool commandProcessed = false;
  if (dataPacket[0] == setBackupOrigin)
  {
    DebugSerial.println("Command Received: Setting node as backup origin.");
    backupOriginNode = true;
    commandProcessed = true;
  }
  if (dataPacket[0] == reinitializeNetwork)
  {
    DebugSerial.println("Command Received: Re-intialize Device");
    DeviceInitialization();
    commandProcessed = true;
  }

  //Clear array for later use.
  ClearNodeArray();

  if (!commandProcessed) {
    DebugSerial.println(F("Command could not be processed. Discarding packet."));
    return false;
  }
  else
  {
    return true;
  }
}

String StringFormatter(String s)
{
	int sLength = s.length();
	int bitsToAdd = 8 - sLength;

	String temp = "";
	for (int i = 0; i < bitsToAdd; i++) {
		temp = temp + "0";
	}
	return temp + s;
}

String Concatenate(uint8_t array[]) {
	String builder = "";
	
	for (int i = 0; i < MAX_NODES; i++)
	{
		if (array[i] != 0)
		{
			builder = builder + StringFormatter(String(array[i], BIN));
		}
	}
	return builder;
}

void HubLoop()
{
  DebugSerial.println("Hub Node: Waiting for data");
  bool packetRecieved = false;
  packetRecieved = WaitForPacket(waitTime);
  String stringToSend;

  if (packetRecieved)
  {
	  stringToSend = Concatenate(dataPacket);
  }

  DebugSerial.print("qwerty" + stringToSend);
  DebugSerial.println("");
  ClearDataArray();
}

bool DeviceInitialization()
{

  //Initialization Period
  unsigned long initializationStartTime = millis();
  bool initializationComplete = false;

  bool gotLocalInformation = false;
  bool deviceConfigurationComplete = false;
  bool discoveredNodes = false;
  bool neighborsSelected = false;
  bool originNodeResolved = false;
  bool sleepModeEnabled = false;
  bool networkDiscoveryOptionsSet = false;

  //Determine this nodes SerialHigh and SerialLow. This will be needed later to identify the NodeIdentifier
  //during the discovery step.
  do
  {
    gotLocalInformation = GetLocalNodeInformation();
  } while (millis() - initializationStartTime < deviceInitializationTime && !gotLocalInformation);

  do
  {

	  if (!networkDiscoveryOptionsSet) 
	  {
		  //This will make the XBee respond to the Network Discovery command so we can determine the
		  //identifier.
		  uint8_t data = 0x02;
		  DebugSerial.println(F("NetworkDiscoveryOptions(NO) command sent to Xbee."));
		  networkDiscoveryOptionsSet = SendATCommand(NO, data);
	  }

	  //if (!sleepModeEnabled) 
	  //{
		//  uint8_t data = 0x01;
		 // DebugSerial.println(F("SleepMode(SM) command sent to Xbee."));
		 // sleepModeEnabled = SendATCommand(SM, data);
	 // }


	  if (networkDiscoveryOptionsSet)
		  deviceConfigurationComplete = true;

  } while (millis() - initializationStartTime < deviceInitializationTime && !deviceConfigurationComplete);

  //Finds all nodes within the network. Then we can determine whether or not the node is
  //the origin.
  do
  {
    discoveredNodes = NetworkDiscovery();
    SortNodeArray();
    PrintNodeArray();

    //Check to see if this is the origin node.
    originNodeResolved = IsOriginNode();
    IsHubNode();

  } while (millis() - initializationStartTime < deviceInitializationTime && !discoveredNodes || !originNodeResolved);

  //We need to clear the array so it can be repopulated during the FN phase.
  ClearNodeArray();

  //Find all neighbors and then sort the nodes by NodeIdentifier. This will make it easier
  //to handle failures as we can simply increment through the array to find potential next
  //hops. Note that netowrkd discovery is redone if no suitable neighbors are found in the hopes
  //that we get a suitable reply in the subsequent broadcasts.
  do
  {

    //If the node is the Hub. it should not need to select neighbors.
    if(!isHub)
    {
      discoveredNodes = FindNodes();
      SortNodeArray();
  
      //Need to determine how many of the neighbors we want to send to. The default range will be within 3
      //hops of the current node, in the direction of the hub.
      neighborsSelected = SelectNeighbors();
  
      //Last initialization comdition complete
      if (neighborsSelected)
        initializationComplete = true; 
    }
    else
    {
      neighborsSelected = true;
      initializationComplete = true;
    }
  } while (millis() - initializationStartTime < deviceInitializationTime && !neighborsSelected);

  //Print all found nodes.
  PrintNodeArray();

  //Finally print our Local Information.
  DebugSerial.println(F("Local Node Information."));
  DebugSerial.print(F("SH: "));
  DebugSerial.print(LocalNode.serialNumberHigh, HEX);
  DebugSerial.print(F(";  SL: "));
  DebugSerial.print(LocalNode.serialNumberLow, HEX);
  DebugSerial.print(F(";  NI: "));
  DebugSerial.println(LocalNode.nodeIdentifier, DEC);
  DebugSerial.println("");

  //This is a catch for if we failed to intialize properly in the alotted time.
  while (!initializationComplete)
  {
    delay(1000);
    DebugSerial.println(F("Initialization failed. Shutting down XBee."));

	//Setting pin high to sleep XBee.
	pinMode(XBEE_SLEEPRQ_PIN, OUTPUT);
	digitalWrite(XBEE_SLEEPRQ_PIN, HIGH);
  }
  DebugSerial.print(F("At initialization complete transmitDataIndex: "));
  DebugSerial.println(transmitDataIndex);
  DebugSerial.println(F("All initialization steps complete. Waiting to start."));

  //This is an extra layer, that is meant to prevent the need for interrupts in the previous loop.
  do {} while (millis() - initializationStartTime < (deviceInitializationTime + 5000));
  return true;
}

//Main Functions=============================================================================


void setup()
{


  // Initiliaze the hardware and software serial.
  DebugSerial.begin(115200);
  randomSeed(analogRead(0));
  XBeeSerial.begin(9600);
  xbee.setSerial(XBeeSerial);

  //Wake up command in case it was asleep.
  pinMode(XBEE_SLEEPRQ_PIN, OUTPUT);
  digitalWrite(XBEE_SLEEPRQ_PIN, LOW);

  //Main Setup Function. Will run until setup is completed, or until 5 tries has been completed.
  //If it fails after 5 tries the device will not be to work and will attempt to broadcast a message
  //to the hub informing of the failure.
  int count = 0;
  do
  {
    deviceSetupComplete = DeviceInitialization();
    count++;

  } while (!deviceSetupComplete && count < 5);


  ///// Sensor Integration Code /////
  //pinMode(13, OUTPUT); //LED
  //pinMode(12, OUTPUT);
  //digitalWrite(12, LOW);
  ///// Sensor Integration Code End /////
  DebugSerial.println(F("Network loop starting now..."));

}

void loop()
{
  xbee.loop();
  bool packetRecieved = false;
  bool packetTransmitted = false;

  //Store the intial start time.
  cycleStartTime = millis();

  if (originNode)
  {
    do
    {
		AppendSensorData();
      packetTransmitted = TransmitData();
    } while (!packetTransmitted);
    do {} while (millis() - cycleDuration < cycleStartTime);
  }
  else if (isHub)
  {
    HubLoop();
  }
  else
  {
    if (backupOriginNode)
    {
      DebugSerial.println("Backup Origin: Waiting for data");
      packetRecieved = WaitForPacket(waitTime);
      if (packetRecieved && !packetTransmitted)
      {
        DebugSerial.println("Data recieved and processed. Preparing to send.");
        originMissCount = 0;
        AppendSensorData();
        packetTransmitted = TransmitData();
      }
      else
      {
        DebugSerial.println(F("Nothing heard from origin. Incrementing miss counter."));
        originMissCount++;
        if (originMissCount == 3)
        {
          DebugSerial.println(F("Too many cycles without hearing from origin. Backup promoted to origin."));
          originNode = true;
          backupOriginNode = false;
          originMissCount = 0;
          CommandNewBackupOrigin();
        }
      }
    }
    else
    {
      DebugSerial.println("Node: Waiting for data");
      packetRecieved = WaitForPacket(waitTime);
      if (packetRecieved && !packetTransmitted)
      {
        AppendSensorData();
        packetTransmitted = TransmitData();
      }
      else
      {
        cycleMissCount++;
        if (cycleMissCount == 5)
        {
          //Nothing heard from any other nodes for 5 cycles.
          //Assume network partition and force network to re-initialize.

        }
      }
    }
  }
}
