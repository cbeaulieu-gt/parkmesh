//#include <ForwardingHandler.h>
#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include <Node.h>

// DONT CHANGE THESE DECLARATIONS++++++++++++++++++

AltSoftSerial SoftSerial;
XBeeWithCallbacks xbee;
#define DebugSerial Serial
#define XBeeSerial SoftSerial
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error This code relies on little endian integers!
#endif

//+++++++++++++++++++++++++++++++++++++++++++++++++

bool debugModeEnabled = true;


const uint8_t MAX_NODES = 10;
Node LocalNode;
Node AllNodes[MAX_NODES];
Node DestinationNodes[3];
int neighbors = 0;

uint8_t transmitData[MAX_NODES];
int transmitDataIndex = 0;

long defaultWait = 0;
unsigned long last_tx_time = 0;

//Commands
uint8_t FN[] = { 'F', 'N' }; //Find Network Devices
uint8_t ND[] = { 'N', 'D' }; //Network Discovery
uint8_t NO[] = { 'N', 'O' }; //Network Discovery Options
uint8_t SH[] = { 'S', 'H' }; //Serial High
uint8_t SL[] = { 'S', 'L' }; //Serial Low


// For now we are counting down towards the hub. This means that the origin
// node will have a Node ID equal to that of the MAX_NODES in the network.
bool isOrigin = true;

// This function will allow for all debug statements to be toggled On/Off
// using the debugModeEnabled boolean.
void DebugSerialTest(char* value)
{
	if (debugModeEnabled)
	{
		DebugSerial.print(value);
	}
}

void DebugSerialTest(int value, char* type)
{
	if (debugModeEnabled)
	{
		if (type == "HEX")
			DebugSerial.print(value, HEX);
		else if (type == "OCT")
			DebugSerial.print(value, OCT);
		else if (type == "DEC")
			DebugSerial.print(value, DEC);
		else
			DebugSerial.print(value, DEC);
	}
}

void GetLocalNodeInformation() 
{
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
}

void SetNetworkDiscoveryOptions() 
{
	AtCommandRequest atCommandRequest = AtCommandRequest(NO);
	AtCommandResponse atCommandResponse = AtCommandResponse();

	//This will make the XBee respond to the Network Discovery command so we can determine the
	//identifier.
	uint8_t data = 0x02;
	atCommandRequest.setCommandValue(&data);
	atCommandRequest.setCommandValueLength(sizeof(data));

	//Send Network Discovery Options Command
	xbee.send(atCommandRequest);
	DebugSerial.println(F("NetworkDiscoveryOptions(NO) command sent to Xbee."));

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
}

//Prints the array of nodes found in the FindNeighbor(FN) command.
void PrintNodeArray() 
{
	for (Node n : AllNodes)
	{
		DebugSerial.print(F("SH: "));
		DebugSerial.print(n.serialNumberHigh, HEX);
		DebugSerial.print(F(";  SL: "));
		DebugSerial.print(n.serialNumberLow, HEX);
		DebugSerial.print(F(";  NI: "));
		DebugSerial.println(n.nodeIdentifier, DEC);
	}
}

//Sorts the array of found nodes by the NodeIdentifiers from largest to smallest.
void SortNodeArray() 
{
	//Implements a simple bubble up function. Note that MAX_NODES is static and defines the
	//size of the array so we dont need to check the length of the array directly.
	for (int ii = 0; ii < MAX_NODES; ii++)
	{
		for (int jj = ii+1; jj < MAX_NODES; jj++) 
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
//the Node Identifier into an array. Note that this does not discover the entire network. 
//Only nodes within a single hop of the source can respond.
void FindNodes()
{
	AtCommandRequest atCommandRequest_FN = AtCommandRequest(ND);
	AtCommandResponse atCommandResponse = AtCommandResponse();
	
	//Send Network Discovery Command
	xbee.send(atCommandRequest_FN);
	DebugSerial.println(F("FindNetwork(FN) command sent to Xbee."));

	int index = 0; 
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
					//because the location of the identifier is variable, and the length can also
					//change once the beginning is found.

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
					}
					index++;
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

	DebugSerial.println(F("No more packet founds."));

}

//Extracts potential destination nodes from the list of all nodes that responded
//to the Network Discovery command.
void SelectNeighbors() 
{
	//Note that the array has been pre-sorted (biggest to smallest) so we can take the
	//first 3 entries we encounter that satisfy the conditions without worrying about 
	//grabbing nodes too far down the list.
	int nodesSelected = 0;
	for (int index = 0; index < MAX_NODES; index++) 
	{
		if (AllNodes[index].nodeIdentifier < LocalNode.nodeIdentifier && nodesSelected < 3 && AllNodes[index].nodeIdentifier != 0) 
		{
			DestinationNodes[nodesSelected] = AllNodes[index];
			DebugSerial.println(F("Neighbor selected."));
			DebugSerial.print(F("SH: "));
			DebugSerial.print(DestinationNodes[nodesSelected].serialNumberHigh, HEX);
			DebugSerial.print(F(";  SL: "));
			DebugSerial.print(DestinationNodes[nodesSelected].serialNumberLow, HEX);
			DebugSerial.print(F(";  NI: "));
			DebugSerial.println(DestinationNodes[nodesSelected].nodeIdentifier, DEC);

			nodesSelected++;
		}
	}

	//For now this is advisory but in a real network this should cause the node to either shut down
	//or re-initiate a Network Discovery at a later period.
	if (nodesSelected == 0) 
	{
		DebugSerial.println(F("No suitable neighbors found."));
	}
}

//Retrieve data from the sensor device.
int GetSensorData() 
{
	//For now this function will simply get a random piece of data. Either a zero or a one.
	uint8_t randomSignal = random(2);
	DebugSerial.print(F("Generated Signal: "));
	DebugSerial.println(randomSignal);

	return randomSignal;
}

//This function will grab the sensor data and will append it to the end of the data array.
//This should have been previously populated by any forwarding data recieved from nodes further
//down the chain.
void AppendSensorData() 
{
	int sensorData = GetSensorData();

	uint8_t payload = (sensorData << 7) | LocalNode.nodeIdentifier;
	DebugSerial.print(F("Payload : "));
	DebugSerial.println(payload, HEX);

	//There is no need to check for size as the array is predefined
	//to be able to hold an entry for every node in the network and no node will ever be
	//able to speak with all other nodes.
	transmitData[transmitDataIndex] = payload;
}

void SendPacket() 
{
	int index = 0;
	//THe request and the data are independent of the specific node we are trying to send to
	//so they can be delcared outside of the loop
	ZBTxRequest transmitRequest;
	transmitRequest.setPayload(transmitData, sizeof(sizeof(uint8_t)*transmitDataIndex));

	//The status will be used to determine whether or not the transmission was succesfull.
	//For now we start with a value that cannot be returned normally.
	uint8_t status = -1;

	//This loop will attempt to send the data to the next node, whose information is located in
	//the array populated by ND. It will keep attempting to do so until it either succeeds in 
	//transmitting to one, or it reaches the end of the list.
	do
	{
		//These will change based on the target so they need to be declared inside of the loop.
		uint64_t address = (uint64_t)DestinationNodes[index].serialNumberHigh << 32 | (uint64_t)DestinationNodes[index].serialNumberLow;
		transmitRequest.setAddress64(address);

		//For now wait 5 seconds. Might want to lower this for the full network configuration.
		status = xbee.sendAndWait(transmitRequest, 5000);
		
		DebugSerial.print(F("Target Information: "));
		DebugSerial.print(F("Node Identifier:"));
		DebugSerial.println(DestinationNodes[index].nodeIdentifier);
		DebugSerial.print(F("Serial High:"));
		DebugSerial.println(DestinationNodes[index].serialNumberHigh, HEX);
		DebugSerial.print(F("Serial Low:"));
		DebugSerial.println(DestinationNodes[index].serialNumberLow, HEX);

		if (status != 0)
		{
			DebugSerial.println("Transmission Error.");
		}	
		else
		{
			DebugSerial.println("Transmission Success.");
		}

		DebugSerial.print(F("Status: 0x"));
		DebugSerial.println(status, HEX);

		//DON'T FORGET to increment index.
		index++;
		
	} while (status != 0 && index < sizeof(DestinationNodes));
}
/*
bool recievePacket()
{
	DebugSerial.println(F("Calling Recieve Packet"));
	// Wait up to 1 second for more data before writing out the line
	uint32_t start = millis();
	while (!XBeeSerial.available() && (millis() - start) < 1000);
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
		while (!XBeeSerial.available() && (millis() - start) < 1000);
	}

	DebugSerial.println(F("Found start of data. Beginning to populate forwarding array."));
	forwardingDataIndex = 0;
	uint8_t b;
	while (XBeeSerial.available())
	{
		if (forwardingDataIndex == 0)
			b = XBeeSerial.read();

		forwardingData[forwardingDataIndex++] = b;
		b = XBeeSerial.read();
		DebugSerial.println(b, HEX);
	}
	DebugSerial.println(F("Finished reading data."));
	// Forward any data from the computer directly to the XBee module
	if (DebugSerial.available())
		XBeeSerial.write(DebugSerial.read());
	return true;
}*/

//Functions required by Arduino.

void setup()
{

	// Initiliaze the hardware and software serial.
	DebugSerial.begin(115200);
	randomSeed(analogRead(0));
	XBeeSerial.begin(9600);
	xbee.setSerial(XBeeSerial);

	//Determine this nodes SerialHigh and SerialLow. This will be needed later to identify the NodeIdentifier
	//during the discovery step.
	GetLocalNodeInformation();
	SetNetworkDiscoveryOptions();

	//Find all neighbors and then sort the nodes by NodeIdentifier. This will make it easier
	//to handle failures as we can simply increment through the array to find potential next
	//hops.
	FindNodes();
	SortNodeArray();
	PrintNodeArray();

	//Need to determine how many of the neighbors we want to send to. The default range will be within 3
	//hops of the current node, within the direction of the hub.
	SelectNeighbors();

	//Finally print our Local Information.
	DebugSerial.println(F("Local Node Information."));
	DebugSerial.print(F("SH: "));
	DebugSerial.print(LocalNode.serialNumberHigh, HEX);
	DebugSerial.print(F(";  SL: "));
	DebugSerial.print(LocalNode.serialNumberLow, HEX);
	DebugSerial.print(F(";  NI: "));
	DebugSerial.println(LocalNode.nodeIdentifier, DEC);
	DebugSerial.println("");

}

void loop() 
{

	// Check the serial port to see if there is a new packet available
	xbee.loop();

	// Send a packet every 10 seconds
	if (millis() - last_tx_time > 10000) {
	  last_tx_time = millis();
	  SendPacket();
	}

}
