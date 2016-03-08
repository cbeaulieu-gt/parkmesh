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
// This example shows how you can scan a ZigBee network for the presence
// of other ZigBee devices, and query them for the services they
// support. This example only works on XBee ZB devices, since it uses
// the ZigBee protocol.
//
// This same sketch is also included in the xbee-arduino library, so you
// might find a more recent version there (but you will have to set up
// the correct serial ports yourself, and double-check the baud rate
// used).

#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include "zigbee.h"

// Serial ports to use
AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error This code relies on little endian integers!
#endif

XBeeWithCallbacks xbee;

/** Helper to generate sequential Zdo transaction identifiers */
uint8_t getNextTransactionId() {
  static uint8_t id = 0;
  return id++;
}

#ifndef lengthof
#define lengthof(x) (sizeof(x)/sizeof(*x))
#endif

/**
 * Helper function to print a field name, followed by the hexadecimal
 * value and a newline.
 */
template <typename T>
static void printField(const __FlashStringHelper *prefix, T data);
template <typename T>
static void printField(const __FlashStringHelper *prefix, T data) {
	DebugSerial.print(prefix);
	printHex(DebugSerial, data);
	DebugSerial.println();
}

void printActiveEndpoints(const zdo_active_ep_rsp_header_t *rsp) {
  DebugSerial.println(F("Active endpoints response"));
  printField(F("  About: 0x"), rsp->network_addr_le);
  DebugSerial.print(F("  Endpoints found: 0x"));
  printHex(DebugSerial, rsp->endpoints, rsp->ep_count, F(", 0x"), NULL);
  DebugSerial.println();
}

void printClusters(const __FlashStringHelper *prefix, uint16_t* clusters, uint8_t count) {
  DebugSerial.print(prefix);
  for (uint8_t i = 0; i < count; ++i) {
    if (i > 0) DebugSerial.print(F(", "));
    DebugSerial.print(F("0x"));
    printHex(DebugSerial, ((uint16_t*)clusters)[i]);
  }
  if (!count) DebugSerial.print(F("none"));

  DebugSerial.println();
}

void printSimpleDescriptor(zdo_simple_desc_resp_header_t *rsp) {
  zdo_simple_desc_header_t *desc = (zdo_simple_desc_header_t*)((uint8_t*)rsp + sizeof(zdo_simple_desc_resp_header_t));
  uint8_t *clusters = ((uint8_t*)desc + sizeof(zdo_simple_desc_header_t));

  DebugSerial.println(F("Simple descriptor response"));
  printField(F("  About: 0x"), rsp->network_addr_le);
  printField(F("  Endpoint: 0x"), desc->endpoint);
  printField(F("  Profile ID: 0x"), desc->profile_id_le);
  printField(F("  Device ID: 0x"), desc->device_id_le);
  printField(F("  Device Version: "), (uint8_t)(desc->device_version & 0xf));

  uint8_t ip_count = *clusters++;
  printClusters(F("  Input clusters: "), (uint16_t*)clusters, ip_count);
  clusters += 2*ip_count;
  uint8_t op_count = *clusters++;
  printClusters(F("  Output clusters: "), (uint16_t*)clusters, op_count);
}

void toggle(XBeeAddress64& addr) {
  uint8_t payload[] = {0x01, 0x00, 0x02};
  ZBExplicitTxRequest tx(addr, 0xfffe, 0, 0, payload, sizeof(payload), 0, 9, 9, 0x0006, 0x0104) ;
  tx.setFrameId(xbee.getNextFrameId());
  xbee.send(tx);
}

/* Matching function that can be passed to waitFor() that matches
 * replies to Zdo requests. The data passed along is the Zdo transaction
 * id that was used for the request, which will be used to select the
 * right reply.
 */
bool matchZdoReply(ZBExplicitRxResponse& rx, uintptr_t data) {
  uint8_t *payload = rx.getFrameData() + rx.getDataOffset();
  uint8_t transactionId = (uint8_t)data;

  return rx.getSrcEndpoint() == 0 &&
         rx.getDstEndpoint() == 0 &&
         rx.getProfileId() == WPAN_PROFILE_ZDO &&
         payload[0] == transactionId;
}

/**
 * Create a tx request to send a Zdo request.
 */
ZBExplicitTxRequest buildZdoRequest(XBeeAddress64 addr, uint16_t cluster_id, uint8_t *payload, size_t len) {
  ZBExplicitTxRequest tx(addr, payload, len);
  tx.setSrcEndpoint(WPAN_ENDPOINT_ZDO);
  tx.setDstEndpoint(WPAN_ENDPOINT_ZDO);
  tx.setClusterId(cluster_id);
  tx.setProfileId(WPAN_PROFILE_ZDO);
  tx.setFrameId(xbee.getNextFrameId());
  return tx;
}

/**
 * Create a zdo request, send it and wait for a reply (which will be
 * stored in the given response object).
 * Returns true when a response was received, returns false if something
 * goes wrong (an error message will have been prined already).
 */
bool handleZdoRequest(const __FlashStringHelper *msg, ZBExplicitRxResponse& rx, XBeeAddress64 addr, uint16_t cluster_id, uint8_t *payload, size_t len) {
  ZBExplicitTxRequest tx = buildZdoRequest(addr, cluster_id, (uint8_t*)payload, len);
  xbee.send(tx);

  uint8_t transaction_id = payload[0];
  // This waits up to 5000 seconds, since the default TX timeout (NH
  // value of 1.6s, times three retries) is 4.8s.
  uint8_t status = xbee.waitFor(rx, 5000, matchZdoReply, transaction_id, tx.getFrameId());
  switch(status) {
    case 0: // Success
      return true;
    case XBEE_WAIT_TIMEOUT:
      DebugSerial.print(F("No reply received from 0x"));
      printHex(DebugSerial, addr.getMsb());
      printHex(DebugSerial, addr.getLsb());
      DebugSerial.print(F(" while "));
      DebugSerial.print(msg);
      DebugSerial.println(F("."));
      return false;
    default:
      DebugSerial.print(F("Failed to send to 0x"));
      printHex(DebugSerial, addr.getMsb());
      printHex(DebugSerial, addr.getLsb());
      DebugSerial.print(F(" while "));
      DebugSerial.print(msg);
      DebugSerial.print(F(". Status: 0x"));
      printHex(DebugSerial, status);
      DebugSerial.println();
      return false;
  }
}

/**
 * Request a list of active endpoints from the node with the given
 * address. Print the endpoints discovered and then request more details
 * for each of the endpoints and print those too.
 */
void get_active_endpoints(XBeeAddress64& addr, uint16_t addr16) {
  zdo_active_ep_req_t payload = {
    .transaction = getNextTransactionId(),
    .network_addr_le = addr16,
  };
  printField(F("Discovering services on 0x"), addr16);

  ZBExplicitRxResponse rx;
  if (!handleZdoRequest(F("requesting active endpoints"),
                        rx, addr, ZDO_ACTIVE_EP_REQ,
                        (uint8_t*)&payload, sizeof(payload)))
    return;

  zdo_active_ep_rsp_header_t *rsp = (zdo_active_ep_rsp_header_t*)(rx.getFrameData() + rx.getDataOffset());

  if (rsp->status) {
    printField(F("Active endpoints request rejected. Status: 0x"), rsp->status);
    return;
  }

  printActiveEndpoints(rsp);

  // Copy the endpoint list, since requesting a descriptor below will
  // invalidate the data in rx / rsp.
  uint8_t endpoints[rsp->ep_count];
  memcpy(endpoints, rsp->endpoints, sizeof(endpoints));

  // Request the simple descriptor for each endpoint
  for (uint8_t i = 0; i < sizeof(endpoints); ++i)
    get_simple_descriptor(addr, addr16, endpoints[i]);
}

void get_simple_descriptor(XBeeAddress64& addr, uint16_t addr16, uint8_t endpoint) {
  zdo_simple_desc_req_t payload = {
    .transaction = getNextTransactionId(),
    .network_addr_le = addr16,
    .endpoint = endpoint,
  };

  ZBExplicitRxResponse rx;
  if (!handleZdoRequest(F("requesting simple descriptor"),
                        rx, addr, ZDO_SIMPLE_DESC_REQ,
                        (uint8_t*)&payload, sizeof(payload)))
    return;

  zdo_simple_desc_resp_header_t *rsp = (zdo_simple_desc_resp_header_t*)(rx.getFrameData() + rx.getDataOffset());

  if (rsp->status) {
    printField(F("Failed to fetch simple descriptor. Status: 0x"), rsp->status);
    return;
  }

  printSimpleDescriptor(rsp);
}

bool getAtValue(uint8_t cmd[2], uint8_t *buf, size_t len, uint16_t timeout = 150) {
  AtCommandRequest req(cmd);
  req.setFrameId(xbee.getNextFrameId());
  uint8_t status = xbee.sendAndWait(req, timeout);
  if (status != 0) {
    DebugSerial.print(F("Failed to read "));
    DebugSerial.write(cmd, 2);
    DebugSerial.print(F(" command. Status: 0x"));
    DebugSerial.println(status, HEX);
    return false;
  }

  AtCommandResponse response;
  xbee.getResponse().getAtCommandResponse(response);
  if (response.getValueLength() != len) {
    DebugSerial.print(F("Unexpected response length in "));
    DebugSerial.write(cmd, 2);
    DebugSerial.println(F(" response"));
    return false;
  }

  memcpy(buf, response.getValue(), len);
  return true;
}

// Invert the endianness of a given buffer
void invertEndian(uint8_t *buf, size_t len) {
  for (uint8_t i = 0, j = len - 1; i < len/2; ++i, j--) {
    uint8_t tmp = buf[i];
    buf[i] = buf[j];
    buf[j] = tmp;
  }
}

/**
 * Struct to keep info about discovered nodes.
 */
struct node_info {
  XBeeAddress64 addr64;
  uint16_t addr16;
  uint8_t typC: 2;
  uint8_t visited: 1;
};

/**
 * List of nodes found.
 */
node_info nodes[10];
uint8_t nodes_found = 0;

/**
 * Scan the network and discover all other nodes by traversing neighbour
 * tables. The discovered nodes are stored in the nodes array.
 */
void scan_network() {
  DebugSerial.println();
  DebugSerial.println("Discovering devices");
  // Fetch our operating PAN ID, to filter the LQI results
  uint8_t pan_id[8];
  getAtValue((uint8_t*)"OP", pan_id, sizeof(pan_id));
  // XBee sends in big-endian, but ZDO requests use little endian. For
  // easy comparsion, convert to little endian
  invertEndian(pan_id, sizeof(pan_id));

  // Fetch the addresses of the local node
  XBeeAddress64 local;
  uint8_t shbuf[4], slbuf[4], mybuf[2];
  if (!getAtValue((uint8_t*)"SH", shbuf, sizeof(shbuf)) ||
      !getAtValue((uint8_t*)"SL", slbuf, sizeof(slbuf)) ||
      !getAtValue((uint8_t*)"MY", mybuf, sizeof(mybuf)))
    return;

  nodes[0].addr64.setMsb((uint32_t)shbuf[0] << 24 | (uint32_t)shbuf[1] << 16 | (uint32_t)shbuf[2] << 8 | shbuf[3]);
  nodes[0].addr64.setLsb((uint32_t)slbuf[0] << 24 | (uint32_t)slbuf[1] << 16 | (uint32_t)slbuf[2] << 8 | slbuf[3]);
  nodes[0].addr16 = (uint16_t)mybuf[0] << 8 | mybuf[1];
  nodes[0].type = ZDO_MGMT_LQI_REQ_TYPE_UNKNOWN;
  nodes[0].visited = false;
  nodes_found = 1;

  DebugSerial.print(F("0) 0x"));
  printHex(DebugSerial, nodes[0].addr64);
  DebugSerial.print(F(" (0x"));
  printHex(DebugSerial, nodes[0].addr16);
  DebugSerial.println(F(", Self)"));

  // nodes[0] now contains our own address, the rest is invalid. We
  // explore the network by asking for LQI info (neighbour table).
  // Initially, this pretends to send a packet to ourselves, which the
  // XBee firmware conveniently handles by pretending that a reply was
  // received (with one caveat: it seems the reply arrives _before_ the
  // TX status).
  uint8_t next = 0;
  do {
    // Query node i for its LQI table
    zdo_mgmt_lqi_req_t payload = {
      .transaction = getNextTransactionId(),
      .start_index = 0,
    };

    do {
      ZBExplicitRxResponse rx;
      if (!handleZdoRequest(F("requesting LQI/neighbour table"),
                            rx, nodes[next].addr64, ZDO_MGMT_LQI_REQ,
                            (uint8_t*)&payload, sizeof(payload)))
        break;

      zdo_mgmt_lqi_rsp_t *rsp = (zdo_mgmt_lqi_rsp_t*)(rx.getFrameData() + rx.getDataOffset());
      if (rsp->status != 0) {
        if (rsp->status != ZDO_STATUS_NOT_SUPPORTED) {
          DebugSerial.print(F("LQI query rejected by 0x"));
          printHex(DebugSerial, nodes[next].addr16);
          DebugSerial.print(F(". Status: 0x"));
          printHex(DebugSerial, rsp->status);
          DebugSerial.println();
        }
        break;
      }

      if (rsp->start_index != payload.start_index) {
        DebugSerial.println(F("Unexpected start_index, skipping this node"));
        break;
      }

      for (uint8_t i = 0; i < rsp->list_count; ++i) {
        zdo_mgmt_lqi_entry_t *e = &rsp->entries[i];
        node_info *n = &nodes[nodes_found];

        if (memcmp(&e->extended_pan_id_le, &pan_id, sizeof(pan_id)) != 0) {
          DebugSerial.println(F("Ignoring node in other PAN"));
          continue;
        }

        // Skip if we know about this node already
        uint8_t dup;
        for (dup = 0; dup < nodes_found; ++dup) {
          if (nodes[dup].addr16 == e->nwk_addr_le)
            break;
        }
        if (dup != nodes_found)
          continue;

        n->addr64.setMsb(e->extended_addr_le >> 32);
        n->addr64.setLsb(e->extended_addr_le);
        n->addr16 = e->nwk_addr_le;
        n->type = e->flags0 & 0x3;

        DebugSerial.print(nodes_found);
        DebugSerial.print(F(") 0x"));
        printHex(DebugSerial, n->addr64);
        DebugSerial.print(F(" (0x"));
        printHex(DebugSerial, n->addr16);
        switch (n->type) {
          case ZDO_MGMT_LQI_REQ_TYPE_COORDINATOR:
            DebugSerial.println(F(", Coordinator)"));
            break;
          case ZDO_MGMT_LQI_REQ_TYPE_ROUTER:
            DebugSerial.println(F(", Router)"));
            break;
          case ZDO_MGMT_LQI_REQ_TYPE_ENDDEVICC:
            DebugSerial.println(F(", End device)"));
            break;
          case ZDO_MGMT_LQI_REQ_TYPE_UNKNOWN:
            DebugSerial.println(F(", Unknown)"));
            break;
        }
        nodes_found++;

        if (nodes_found == lengthof(nodes)) {
          DebugSerial.println(F("Device table full, terminating network scan"));
          return;
        }
      }

      // Got all neighbours available? Done.
      if (rsp->start_index + rsp->list_count >= rsp->table_entries)
        break;
      // More left? Loop and get more.
      payload.start_index += rsp->list_count;
      payload.transaction = getNextTransactionId();
    } while (true);

    // Done with this node, on to the next
    nodes[next].visited = true;
    ++next;
  } while (next < nodes_found);
  DebugSerial.println(F("Finished scanning"));
  DebugSerial.println(F("Press a number to scan that node, or press r to rescan the network"));
}

void setup() {
  DebugSerial.begin(115200);

  XBeeSerial.begin(9600);
  xbee.setSerial(XBeeSerial);

  xbee.onPacketError(printErrorCb, (uintptr_t)(Print*)&DebugSerial);

  // Set AO=1 to receive explicit RX frames
  // Because this does not write to flash with WR, AO should be reverted
  // on reboot.
  uint8_t value = 1;
  AtCommandRequest req((uint8_t*)"AO", &value, sizeof(value));
  req.setFrameId(xbee.getNextFrameId());
  uint8_t status = xbee.sendAndWait(req, 150);
  if (status == 0)
    DebugSerial.println(F("Set AO=1"));
  else
    DebugSerial.println(F("Failed to set AO, expect problems"));

  scan_network();
}


void loop() {
  // Read serial to see if a node was chosen. If so, start a closer scan
  // of that node and rescan.
  if (DebugSerial.available()) {
    uint8_t c = DebugSerial.read();
    if (c >= '0' && c <= '9') {
      int n = c - '0';
      if (n < nodes_found) {
        get_active_endpoints(nodes[n].addr64, nodes[n].addr16);
        scan_network();
      }
    } else if (c == 'r') {
      scan_network();
    }
  }

  xbee.loop();
}
