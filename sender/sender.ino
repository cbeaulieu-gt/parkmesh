#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include <Node.h>
// Serial ports to use
AltSoftSerial SoftSerial;

#define DebugSerial Serial
#define XBeeSerial SoftSerial

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error This code relies on little endian integers!
#endif
const uint8_t MAX_NET_RANGE = 2;
const uint8_t MAX_NODES = 2;
Node nodes[MAX_NET_RANGE];
uint8_t forwardingData[MAX_NODES];
int forwardingDataIndex;
XBeeWithCallbacks xbee;
uint32_t _myNI = 0;
uint8_t cmd[] = {'F', 'N'};
bool isOrigin = true;
long defaultWait = 0;

AtCommandResponse atResponse = AtCommandResponse();
void setup() {
  // put your setup code here, to run once:
  DebugSerial.begin(115200);
  randomSeed(analogRead(0));
  XBeeSerial.begin(9600);
  xbee.setSerial(XBeeSerial);
  getNI();
  DebugSerial.println(_myNI);
  AtCommandRequest atRequest = AtCommandRequest(cmd);
  //Send Network Discovery Command
  xbee.send(atRequest);
  DebugSerial.write(cmd, 2);
  DebugSerial.println(F(" command sent to Xbee"));

  findNodes();
  for (Node n : nodes)
  {
    DebugSerial.print(F("SH: "));
    DebugSerial.print(n._sh, HEX);
    DebugSerial.print(F(";  SL: "));
    DebugSerial.print(n._sl, HEX);
    DebugSerial.print(F(";  NI: "));
    DebugSerial.println(n._ni);
  }
  // GetResponse();
  //sendPacket();
}

void getNI()
{
  uint8_t niCmd[] = {'N', 'I'};
  AtCommandRequest req(niCmd);
  xbee.send(req);
  while (xbee.readPacket(5000))
  {
    xbee.getResponse().getAtCommandResponse(atResponse);
    uint8_t val;
    for (int i = 0; i < atResponse.getValueLength(); i++)
    {
      val = atResponse.getValue()[i];
      if (val != 0x2D)
      {
        _myNI = _myNI * 10 + (val - 0x30);
      }
      else {
        break;
      }
    }
  }
}
void findNodes()
{
  int index = 0;
  while (xbee.readPacket(5000))
  {

    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
    {
      xbee.getResponse().getAtCommandResponse(atResponse);

      //Check status of response.
      DebugSerial.println(atResponse.getStatus(), HEX);

      //Check to see if the command we got a response for matches what was expected.
      if (atResponse.isOk())
      {
        DebugSerial.write(cmd, 2);
        DebugSerial.println(F(" Command was successful."));

        if (atResponse.getValueLength() > 0)
        {
          DebugSerial.println(F("Command value length is "));
          DebugSerial.println(atResponse.getValueLength(), DEC);

          DebugSerial.println(F("Command value: "));
          uint32_t sh = 0;
          uint32_t sl = 0;
          for (int i = 2; i < 6; i++)
          {
            sh = sh << 8;
            sh = sh | atResponse.getValue()[i];
          }
          for (int i = 6; i < 10; i++)
          {
            sl = sl << 8;
            sl = sl | atResponse.getValue()[i];
          }

          uint32_t ni = 0;
          uint8_t val;
          for (int i = 10; i < atResponse.getValueLength(); i++)
          {
            val = atResponse.getValue()[i];
            if (val != 0x2D)
            {
              ni = ni * 10 + (val - 0x30);
            }
            else {
              break;
            }
          }
            nodes[index]._sh  = sh;
            DebugSerial.print(F("SH: "));
            DebugSerial.println(sh, HEX);

            nodes[index]._sl = sl;
            DebugSerial.print(F("SL: "));
            DebugSerial.println(sl, HEX);

            nodes[index]._ni = ni;
            DebugSerial.print(F("NI: "));
            DebugSerial.println(ni);
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

  DebugSerial.println(F("No more packet found."));
  //Look for RemoteCommandResponse Packet
}
void findNodesunsuccessful()
{

  while (xbee.readPacket(5000))
  {

    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
    {
      xbee.getResponse().getAtCommandResponse(atResponse);

      //Check status of response.
      DebugSerial.println(atResponse.getStatus(), HEX);

      //Check to see if the command we got a response for matches what was expected.
      if (atResponse.isOk())
      {
        DebugSerial.write(cmd, 2);
        DebugSerial.println(F(" Command was successful."));

        if (atResponse.getValueLength() > 0)
        {

          DebugSerial.println(F("Command value length is "));
          DebugSerial.println(atResponse.getValueLength(), DEC);

          DebugSerial.println(F("Command value: "));
          uint32_t ni = 0;
          uint8_t val;
          for (int i = 10; i < atResponse.getValueLength(); i++)
          {
            val = atResponse.getValue()[i];
            if (val != 0x2D)
            {
              ni = ni * 10 + (val - 0x30);
            }
            else {
              break;
            }
          }
          //if (ni < _myNI && (ni >= (_myNI < MAX_NET_RANGE || ni >= (_myNI - MAX_NET_RANGE))))
          //{
          uint32_t sh = 0;
          uint32_t sl = 0;
          int index = _myNI - ni;
          for (int i = 2; i < 6; i++)
          {
            sh = sh << 8;
            sh = sh | atResponse.getValue()[i];
          }
          for (int i = 6; i < 10; i++)
          {
            sl = sl << 8;
            sl = sl | atResponse.getValue()[i];
          }
          nodes[index]._sh  = sh;
          nodes[index]._sl = sl;
          nodes[index]._ni = ni;
          //}
        }
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
  DebugSerial.println(F("No more packet found."));
  //Look for RemoteCommandResponse Packet
}

void sendPacket() {
  // Prepare the Zigbee Transmit Request API packet
  int nodeInd = 0;
  ZBTxRequest txRequest;
  uint8_t sig = random(2);
  DebugSerial.print(F("Signal: "));
  DebugSerial.println(sig);
  uint8_t niAdjusted = 0x9F & _myNI;
  uint8_t payloadCore = (sig << 7) | niAdjusted;
  DebugSerial.print(F("Payload Combined: "));
  DebugSerial.println(payloadCore, HEX);
  uint8_t payload[] = {payloadCore};
  txRequest.setPayload(payload, sizeof(payload));

  // And send it
  uint8_t status;
  uint64_t addr;
  do {
    DebugSerial.print(F("Sending to "));
    DebugSerial.print(nodes[nodeInd]._ni);

    addr = (uint64_t)nodes[nodeInd]._sh << 32 | (uint64_t)nodes[nodeInd]._sl;
    //DebugSerial.println(n._sh,HEX);
    //DebugSerial.println(n._sl,HEX);
    txRequest.setAddress64(addr);
    status = xbee.sendAndWait(txRequest, 5000);
    DebugSerial.print(F(" . Status: 0x"));
    DebugSerial.println(status, HEX);
    nodeInd++;
  } while (status != 0 && nodeInd <= MAX_NET_RANGE);

}

unsigned long last_tx_time = 0;

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();
  // Send a packet every 10 seconds

  if (millis() - last_tx_time > 10000) {
    last_tx_time = millis();
    sendPacket();
  }

}


