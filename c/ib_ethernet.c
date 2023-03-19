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
  nextPacket += offset;
  assert(nextPacket>packet);
  assert(((uint64_t)(nextPacket) % CPU_CACHE_SIZE)==0);
  
  // OK initialize 
  struct IPV4Packet *packetObj = (struct IPV4Packet *)packet;

  // IP header
  memcpy(packetObj->dstMac, session->serverMac, sizeof(packetObj->dstMac));
  memcpy(packetObj->srcMac, session->clientMac, sizeof(packetObj->srcMac));
  packetObj->ethType = 2048;                // Ethernet

  // IPV4 header
  packetObj->ihl = 5;                       // header is 5 words big
  packetObj->version = 4;                   // Ethernet V4
  packetObj->typeOfService = 0;
  packetObj->ipv4Size = 0                   // size of this header + everything that follows; calc'd below
  packetObj->packetId = 0;                  // packet sequence number; caller can set on return
  packetObj->fragmentOffset = 0;
  packetObj->ttl = 64;
  packetObj->nextProtoId = 0x200;           // next packet will also be IPV4 UDP (and not fragment)
  packetObj->ipv4Checksum = 0;              // checksum: calc'd below
  packetObj->srcIp = session->clientIpAddr;
  packetObj->dstIp = session->serverIpAddr;

  // UDP header
  packetObj->srcPort = session->clientPort;
  packetObj->dstPort = session->serverPort;
  packetObj->udpSize = 0;                   // size of the UDP header and everything that follows
  packetObj->udpCheckum = 0;                // optional checksum on UDP part; leaving 0

  // Now fill in sizes then IPV4 checksum
  packetObj->ipv4Size = 
  packetObj->udpSize = 
  packetObj->ipv4Checksum



