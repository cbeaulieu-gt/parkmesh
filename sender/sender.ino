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


//List of Used AT Commands+++++++++++++++++++++++++

uint8_t FN[] = { 'F', 'N' }; //Find Network Devices
uint8_t ND[] = { 'N', 'D' }; //Network Discovery
uint8_t NO[] = { 'N', 'O' }; //Network Discovery Options
uint8_t SH[] = { 'S', 'H' }; //Serial High
uint8_t SL[] = { 'S', 'L' }; //Serial Low

//List of Internal Commands
struct Commands {
	int SetOrigin = 6;
};
const Commands CommandList;

//Network Variables++++++++++++++++++++++++++++++++

//These relate to other nodes in the network. Put anything here
//that is not specific to this particular node or would need to be set
//for all nodes in the network.

const int MAX_NODES = 10;
Node AllNodes[MAX_NODES];
Node DestinationNodes[3];
const int maxCommandPacketSize = 20;

//This will be changed later, but at worst case each node will know this value.
uint8_t originNodeIdentifier = MAX_NODES; 

//Local Node+++++++++++++++++++++++++++++++++++++++

//Specfic only to this node. Operation specfic variables should be
//placed here.

Node LocalNode;
uint8_t dataPacket[MAX_NODES];
uint8_t commandPacket[maxCommandPacketSize];
int transmitDataIndex = 0;
int neighbors = 0;
unsigned long cycleStartTime = 0;
unsigned long cycleDuration = 0;

// For now we are counting down towards the hub. This means that the origin
// node will set itself if it does not hear from any nodes with a higher
//NI than itself.
bool isOrigin = false;

//Others++++++++++++++++++++++++++++++++++++++++++++

bool debugModeEnabled = true;

//=================================================================================================
//=================================================================================================


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

//Determines if this node is the origin node, i.e. the furthest node away from the hub
//and the node that will be the first to transmit data.
void IsOrigin()
{
	DebugSerial.println(F("Checking to see if this is the origin node."));

	//Scan through all nodes we heard from and compare the Node Identifiers. If after scanning
	//we have found no nodes with a hgiher NI than ourselves than we must be the origin.
	bool originCandidate = true;
	for (int index = 0; index < MAX_NODES; index++)
	{
		DebugSerial.println(AllNodes[index].nodeIdentifier, DEC);
		if (LocalNode.nodeIdentifier < AllNodes[index].nodeIdentifier) 
		{
			originCandidate = false;
			break;
		}
	}

	if (originCandidate) 
	{
		isOrigin = true;
		originNodeIdentifier = LocalNode.nodeIdentifier;
		DebugSerial.println(F("This is the ORIGIN node."));
		DebugSerial.println(F(""));
	}
	else 
	{
		DebugSerial.println(F("This is NOT the origin node."));
		DebugSerial.println(F(""));
	}
}

//Sends a broadcast message to all nodes in the network informing all nodes that this is the origin
//node. Contains the NI of the announcer.s
bool AnnounceOrigin() 
{
	DebugSerial.println(F("Preparing to announce origin node."));

	//Recieving nodes will listen for this code and upon getting it will store this node identifer
	//as the origin node
	uint8_t data[] = { '0', '1', '1','0', LocalNode.nodeIdentifier };
	uint32_t destinationSerialHigh = 0x00000000;
	uint32_t desitnationSerialLow = 0x0000FFFF;
	
	Node broadcastNode = Node();
	broadcastNode.nodeIdentifier = -1;
	broadcastNode.serialNumberHigh = destinationSerialHigh;
	broadcastNode.serialNumberLow = desitnationSerialLow;

	bool announcementSuccessful = SendPacket(broadcastNode, data, sizeof(data));
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

bool SetNetworkDiscoveryOptions() 
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
	DebugSerial.println(F("Node Response List."));

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

//Extracts potential destination nodes from the list of all nodes that responded
//to the Network Discovery command.
bool SelectNeighbors() 
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
		return false;
	}
	else 
	{
		return true;
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
	dataPacket[transmitDataIndex] = payload;
}

//Sends data (both sensor and forwarding data) to the next node in the chain. Will attempt to send
//to each destination node in succession until it either succeeds or runs out of destination nodes.
bool TransmitData()
{
	int index = 0;
	bool packetSent = false;

	//Because of the way we are passing variables we need a temporary array to store the data we are 
	//sending
	uint8_t *temp = new uint8_t[transmitDataIndex];
	for (int index = 0; index < transmitDataIndex; index++) 
	{
		temp[index] = dataPacket[index];
	}

	//This loop will attempt to send the data to the next node, whose information is located in
	//the array of selected neighbors. It will keep attempting to do so until it either succeeds in 
	//transmitting to one, or it reaches the end of the list of potential recipients.
	while (packetSent == false && index < sizeof(DestinationNodes))
	{
		packetSent = SendPacket(DestinationNodes[index], temp, sizeof(temp));
		index++;
	}
}

//Attemmpts to send a single packet. Can do either targeted or broadcast transmissions.
bool SendPacket(Node targetNode, uint8_t data[], uint8_t dataSize) 
{

	//The request and the data are independent of the specific node we are trying to send to
	//so they can be declared outside of the loop.
	ZBTxRequest transmitRequest;
	transmitRequest.setPayload(data, dataSize);

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
		return true;
	}
}

//Waits for a data acket for the specified time interval.
bool WaitForDataPacket(unsigned long waitTime)
{
	return RecievePacket(waitTime);
}

//Waits for a command packet for the specified time interval.
bool WaitForCommandPacket(unsigned long waitTime) 
{
	bool packetRecieved = false;
	packetRecieved = RecievePacket(waitTime);

	//Due to the nature of the callback used we cant tell it to put the data
	//in different spots based on the caller, so we will copy from the data holder
	//to the command one based on the type of packet were expecting.
	if (packetRecieved) 
	{
		for (int index = 0; index < transmitDataIndex; index++) 
		{
			commandPacket[index] = dataPacket[index];
		}

		int recievedCommand = 0;

		//Compare expected command to recieved command to make sure its what we are waiting for.
		for (int index = 0; index < 4; index++)
		{
			recievedCommand = recievedCommand * 10 + (commandPacket[index] - 0x30);
		}

		if (recievedCommand == CommandList.SetOrigin)
		{
			originNodeIdentifier = (commandPacket[4] - 0x30);
			return true;
		}
		else
		{
			return false;
		}
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
	while (!XBeeSerial.available() && (millis() - start) < waitTime);
	if (!XBeeSerial.available()) 
	{
		DebugSerial.println(F("Nothing found. Exiting recievePacket."));
		return false;
	}

	DebugSerial.println(F("Packet found!!"));

	//Wait a little while for the callback to catch up.
	start = millis();
	while ((millis() - start) < 100) {};
	xbee.onZBRxResponse(ProcessRxPacket);
	return true;
}

void ProcessRxPacket(ZBRxResponse& rx, uintptr_t) 
{
	DebugSerial.print(F("Received packet from "));
	printHex(DebugSerial, rx.getRemoteAddress64());
	DebugSerial.println();
	DebugSerial.print(F("Payload: "));
	
	//Grab the data from the stream and place it into a global array
	for (int ii = 0; ii < rx.getDataLength(); ii++)
	{
		dataPacket[ii] = rx.getData(ii);
		DebugSerial.print(rx.getData(ii), HEX);
		transmitDataIndex = ii;
	}
}

//Main Functions=============================================================================

void setup()
{

	//Network Initialization Period
	unsigned long initializationStartTime = millis();
	bool initializationComplete = false;

	// Initiliaze the hardware and software serial.
	DebugSerial.begin(115200);
	randomSeed(analogRead(0));
	XBeeSerial.begin(9600);
	xbee.setSerial(XBeeSerial);

	bool gotLocalInformation = false;
	bool configComplete = false;
	bool discoveredNodes = false;
	bool neighborsSelected = false;
	bool originNodeResolved = false;

	//Determine this nodes SerialHigh and SerialLow. This will be needed later to identify the NodeIdentifier
	//during the discovery step.
	do
	{
		gotLocalInformation = GetLocalNodeInformation();
	} while (millis() - initializationStartTime < 25000 && !gotLocalInformation);
	
	do
	{
		configComplete = SetNetworkDiscoveryOptions();
	} while (millis() - initializationStartTime < 25000 && !configComplete);

	//Find all neighbors and then sort the nodes by NodeIdentifier. This will make it easier
	//to handle failures as we can simply increment through the array to find potential next
	//hops.
	//
	//The reason we enter here on either failure is becuase if we found nodes but none were 
	//considered suitable nieghbors than we need to try to scan the network again in the hopes
	//of finding a node that is closer to the hub than we are.
	do
	{
		discoveredNodes = FindNodes();
		SortNodeArray();

		//Need to determine how many of the neighbors we want to send to. The default range will be within 3
		//hops of the current node, within the direction of the hub.
		neighborsSelected = SelectNeighbors();

	} while (millis() - initializationStartTime < 25000 && !discoveredNodes || !neighborsSelected);
	
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

	do
	{	
		//Check to see if this is the origin node.
		IsOrigin();

		if (isOrigin)
		{
			originNodeResolved = AnnounceOrigin();
		}
		else
		{
			originNodeResolved = WaitForCommandPacket(4000); //Wait for four seconds.
		}

		if (originNodeResolved)
		{
			initializationComplete = true;
		}
	} while (millis() - initializationStartTime < 25000 && !originNodeResolved);

	//This is a catch for if we failed to intialize properly in the alloted time.
	while (!initializationComplete)
	{
		delay(1000);
		DebugSerial.println(F("Initialization failed. Please restart device."));
	}

	DebugSerial.println(F("All initialization steps complete. Waiting to start."));

	//This is an extra layer, that is meant to prevent the need for interrupts in the previous loop.
	do {} while (millis() - initializationStartTime < 30000);

	DebugSerial.println(F("Network loop starting now..."));

}

void loop() 
{
	DebugSerial.println(F("Starting Cycle."));

	xbee.loop();
	bool packetRecieved = false;
	bool packetTransmitted = false;
	int count = 0;
	
	//Store the intial start time. The cycle will last as # seconds
	//where # is the origin NI.
	cycleStartTime = millis();
	cycleDuration = originNodeIdentifier * 1000;
	
	//May put sleep timer here later. For now no sleeping, just waiting.

	if (!isOrigin) 
	{
		do
		{
			packetRecieved = WaitForDataPacket(cycleDuration - 10 * LocalNode.nodeIdentifier);
			if (packetRecieved)
			{
				AppendSensorData();
				packetTransmitted = TransmitData();
			}
			if (packetTransmitted)
				break;
		} while (millis() - cycleDuration < cycleStartTime && !packetTransmitted && !packetRecieved);
	}
	else 
	{
		do
		{
			AppendSensorData();
			packetTransmitted = TransmitData();
			if (packetTransmitted)
				break;
		} while (millis() - cycleDuration < cycleStartTime && !packetTransmitted);
	}
	
	//Wait for any remaining time to expire.
	while (millis() - cycleDuration < cycleStartTime) {};
}