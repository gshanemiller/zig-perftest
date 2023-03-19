#include <ib_ethernet.h>

int ice_ib_make_ipv4packet(const struct *session, const char *payload, uint16_t payloadSizeBytes) {
  assert(session);
  assert(payload);
  assert(payloadSizeBytes>0);
  assert(payloadSizeBytes==session->userParam->payloadSize);

  // Make this packet at next offset in hugepage memory
  uint64_t offset(0);
  char *packet = session->nextPacket; 

  // Find the start of future next packet (the one made on next call to
  // ice_ib_make_ipv4packet) and make sure it's on a on CPU_CACHE_SIZE boundary
  char *nextPacket = packet + sizeof(IPV4Packet) + payloadSizeBytes;
  char *offset = nextPacket & 0x3F;
  if (offset!=0) {
    nextPacket += (CPU_CACHE_LINE_SIZE_BYTES-offset);
  }
  assert(nextPacket>packet);
  assert(((uint64_t)(nextPacket) % CPU_CACHE_LINE_SIZE_BYTES)==0);
  
  // OK initialize 
  struct IPV4Packet *packetObj = (struct IPV4Packet *)packet;

  // IP header
  memcpy(packetObj->ip_header.dstMac, session->serverMac, sizeof(packetObj->ip_header.dstMac));
  memcpy(packetObj->ip_header.srcMac, session->clientMac, sizeof(packetObj->ip_header.srcMac));
  packetObj->ip_header.ethType = 2048;                        // Ethernet

  // IPV4 header
  packetObj->ipv4_header.ihl = 5;                             // header is 5 words big
  packetObj->ipv4_header.version = 4;                         // Ethernet V4
  packetObj->ipv4_header.typeOfService = 0;
  packetObj->ipv4_header.size = 0                             // sizeof header & everything that follows
  packetObj->ipv4_header.packetId = 0;                        // packet sequence number; caller can set on return
  packetObj->ipv4_header.fragmentOffset = 0;
  packetObj->ipv4_header.ttl = 64;
  packetObj->ipv4_header.nextProtoId = 0x200;                 // next packet will also be IPV4 UDP (and not fragment)
  packetObj->ipv4_header.checksum = 0;                        // checksum: calc'd below
  packetObj->ipv4_header.srcIpAddr = session->clientIpAddr;
  packetObj->ipv4_header.dstIpAddr = session->serverIpAddr;

  // UDP header
  packetObj->ipv4udp_header.srcPort = session->clientPort;
  packetObj->ipv4udp_header.dstPort = session->serverPort;
  packetObj->ipv4udp_header.size = 0;                         // sizeof header & everything that follows
  packetObj->ipv4udp_header.checkum = 0;                      // optional checksum on UDP data; leaving 0

  // Fill in sizes
  packetObj->ipv4Size = sizeof(struct IPV4Header) + sizeof(struct IPV4UDPHeader) + payloadSizeBytes;
  packetObj->udpSize = sizeof(struct IPV4UDPHeader) + payloadSizeBytes;

  // Calculate IPV4 header checksum
  uint32_t ip_cksum = 0;
  uint16_t* ptr16 = (uint16_t*)(packet + sizeof(IPHeader));
  ip_cksum += ptr16[0]; ip_cksum += ptr16[1];
  ip_cksum += ptr16[2]; ip_cksum += ptr16[3];
  ip_cksum += ptr16[4];
  ip_cksum += ptr16[6]; ip_cksum += ptr16[7];
  ip_cksum += ptr16[8]; ip_cksum += ptr16[9];
	// Reduce 32 bit checksum to 16 bits and complement it.
  ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) + (ip_cksum & 0x0000FFFF);
  if (ip_cksum > 65535) ip_cksum -= 65535;
  ip_cksum = (~ip_cksum) & 0x0000FFFF;
  if (ip_cksum == 0) ip_cksum = 0xFFFF;
  packetObj->ipv4_header.checksum = (uint16_t)ip_cksum;

  // Copy-in in payload
  memcpy(packetObj->payload, payload, payloadSizeBytes);

  // Now update pointers in session
  session->packet = packet;
  session->nextPacket = nextPacket; 

  return 0;
}
