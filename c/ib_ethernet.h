#pragma once

#include <ib_session.h>

static const uint64_t UDP_HEADER_SIZE_BYTES  = 0;
static const uint16_t IPV4_HEADER_SIZE_BYTES = 0;

#pragma pack(push,1)
struct IPHeader {
  uint8_t     dstMac[6];
  uint8_t     srcMac[6];
  uint16_t    ethType;
};

struct IPV4Header {
  uint8_t     ihl:4;
  uint8_t     version:4;
  uint8_t     typeOfService;
  uint16_t    size;
  uint16_t    packetId;
  uint16_t    fragmentOffset;
  uint8_t     ttl;
  uint8_t     nextProtoId;
  uint16_t    checksum;
  uint32_t    srcIpAddr;
  uint32_t    dstIpAddr;
};

struct IPV4UDPHeader {
  uint16_t    srcPort;
  uint16_t    dstPort;
  uint16_t    size;
  uint16_t    checksum;
};

// Assumes little endian callers
struct IPV4Packet {
  struct IPHeader       ip_header;
  struct IPV4Header     ipv4_header;
  struct IPV4UDPHeader  ipv4udp_header;
  char payload[];
};
#pragma pack(pop)

int ice_ib_make_ipv4packet(const struct *session, const char *payload, uint16_t payloadSizeBytes);
