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
const uint8_t MAX_NODES = 3;
Node nodes[MAX_NODES];
XBeeWithCallbacks xbee;
uint8_t cmd[] = {'F', 'N'};
AtCommandRequest atRequest = AtCommandRequest(cmd);

AtCommandResponse atResponse = AtCommandResponse();

bool isOrigin =true;
long defaultWait = 0;
void setup() {
  // put your setup code here, to run once:
  DebugSerial.begin(115200);
  randomSeed(analogRead(0));

  XBeeSerial.begin(9600);
  xbee.setSerial(XBeeSerial);

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
  sendPacket();
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

void GetResponse()
{
  if (xbee.readPacket(5000))
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
          DebugSerial.print(F("SH: "));
          DebugSerial.println(sh, HEX);

          DebugSerial.print(F("SL: "));
          DebugSerial.println(sl, HEX);

          DebugSerial.print(F("NI: "));
          DebugSerial.println(ni);

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
  else
  {
    DebugSerial.println(F("No packet found."));
  }
  //Look for RemoteCommandResponse Packet
}

void sendPacket() {
  // Prepare the Zigbee Transmit Request API packet
  int altInd = 0;
  ZBTxRequest txRequest;
  uint64_t addr = (uint64_t)nodes[altInd]._sh << 32 | (uint64_t)nodes[altInd]._sl;
  //DebugSerial.println(n._sh,HEX);
  //DebugSerial.println(n._sl,HEX);
  txRequest.setAddress64(addr);
  uint8_t sig = random(2);
  DebugSerial.print(F("Signal: "));
  DebugSerial.println(sig);
  uint8_t niAdjusted = 0x9F & nodes[altInd]._ni;
  uint8_t payloadCore = (sig << 7) | niAdjusted;
  DebugSerial.print(F("Payload Combined: "));
  DebugSerial.println(payloadCore,HEX);
  uint8_t payload[] = {payloadCore};
  txRequest.setPayload(payload, sizeof(payload));

  // And send it
  uint8_t status = xbee.sendAndWait(txRequest, 5000);
  if (status == 0)
  {
    DebugSerial.println(F("Succesfully sent packet"));
  }
  else
  {
    do
    {
      DebugSerial.print(F("Failed to send packet to "));
      DebugSerial.print(nodes[altInd]._ni);
      DebugSerial.print(F(" . Status: 0x"));
      DebugSerial.println(status, HEX);
      altInd++;
      if (altInd >= MAX_NODES)
      {
        break;
      }
    } while (!sendPacketFurther(altInd));
  }
}
bool sendPacketFurther(int index) {
  ZBTxRequest txRequest;
  uint64_t addr = (uint64_t)nodes[index]._sh << 32 | (uint64_t)nodes[index]._sl;
  //DebugSerial.println(n._sh,HEX);
  //DebugSerial.println(n._sl,HEX);
  txRequest.setAddress64(addr);
  uint8_t sig = random(2);
  DebugSerial.print(F("Signal: "));
  DebugSerial.println(sig);
  uint8_t niAdjusted = 0x9F & nodes[index]._ni;
  uint8_t payloadCore = (sig << 7) | niAdjusted;
  DebugSerial.print(F("Payload Combined: "));
  DebugSerial.println(payloadCore,HEX);
  uint8_t payload[] = {payloadCore};
  txRequest.setPayload(payload, sizeof(payload));

  // And send it
  uint8_t status = xbee.sendAndWait(txRequest, 5000);
  if (status == 0)
  {
    DebugSerial.println(F("Succesfully sent packet"));
    return true;
  }
  else
  {
    return false;
  }
}

void serialDump() {
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


