#pragma once

#include <ib_session.h>

// This assumes little endian callers
#pragma pack(push,1)
struct IPV4Packet {
  // IP Header
  uint8_t     dstMac[6];
  uint8_t     srcMac[6];
  uint16_t    ethType;
  // IPV4 Header
  uint8_t     ihl:4;
  uint8_t     version:4;
  uint8_t     typeOfService;
  uint16_t    ipv4Size;
  uint16_t    packetId;
  uint16_t    fragmentOffset;
  uint8_t     ttl;
  uint8_t     nextProtoId;
  uint16_t    ipv4Checksum;
  uint32_t    srcIpAddr;
  uint32_t    dstIpAddr;
  // UDP header
  uint16_t    srcPort;
  uint16_t    dstPort;
  uint16_t    udpSize;
  uint16_t    udpChecksum;
  // UDP payload
  char payload[];
};
#pragma pack(pop)

int ice_ib_make_ipv4packet(const struct *session, const char *payload, uint16_t payloadSizeBytes);
