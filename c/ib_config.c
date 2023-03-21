#include <ib_config.h>

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int ice_ib_config_check_port_device(const struct ibv_context *context, int portId) {
  assert(context);
  assert(portId>0);

  struct ibv_port_attr attr;
  if (ibv_query_port((struct ibv_context *)context, portId, &attr)) {
    int rc = errno;
    fprintf(stderr, "warn : ibv_query_port %d: failed: %s (errno %d)\n", portId, strerror(rc), rc);                            
    return (ICE_IB_ERROR_API_ERROR);
  }

  if (attr.state != IBV_PORT_ACTIVE) {
    fprintf(stderr, "warn : ib_config_init_link_layer: port %d not active (state %d)\n", portId, attr.state);
    return (ICE_IB_ERROR_API_ERROR);
  }

  if (attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
    fprintf(stderr, "warn : ib_config_init_link_layer: port %d not configured for ethernet. set %u\n", portId, attr.link_layer);
    return (ICE_IB_ERROR_API_ERROR);
  }

  return 0;
}

int ice_ib_allocate_huge_memory(uint64_t requestSizeBytes, struct HugePageMemory *memory) {
  assert(requestSize>0);
  assert(memory!=0);
  
  // Round request size to nearest 2MB
  uint64_t buf_size = requestSizeBytes;
  if (buf_size<HUGEPAGE_ALIGN_2MB) {
    buf_size = HUGEPAGE_ALIGN_2MB;
  } else if ((buf_size%HUGEPAGE_ALIGN_2MB)!=0) {
    uint64_t buf_remainder = buf_size & (HUGEPAGE_ALIGN_2MB-1);
    buf_size += (HUGEPAGE_ALIGN_2MB-buf_remainder);
  }
  assert((buf_size%HUGEPAGE_ALIGN_2MB)==0);

  // Request request v. actual memory ask
  memory->requestSizeBytes = requestSizeBytes;
  memory->actualSizeBytes = buf_size;

  fprintf(stderr, "info : ice_ib_allocate_huge_memory: requested %lu bytes rounded to %lu bytes\n",
    requestSizeBytes, buf_size);
    
  // create 2MB huge pages with read/write permissions
  memory->shmid = shmget(IPC_PRIVATE, buf_size, SHM_HUGETLB | SHM_HUGE_2MB | IPC_CREAT | SHM_R | SHM_W);
  if (memory->shmid < 0) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_huge_memory: cannot allocate %lu bytes of hugepage memory: %s (errno %d)\n",
      buf_size, strerror(rc), rc);
    assert(rc!=0);
    return rc;
  }

  // attach shared memory
  memory->hugePageMemory = (void *)shmat(memory->shmid, 0, 0);
  if (memory->hugePageMemory==(void*)(-1)) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_huge_memory: cannot attach %lu bytes of hugepage memory: %s (errno %d)\n",
      buf_size, strerror(rc), rc);
    memory->hugePageMemory = 0;
    assert(rc!=0);
    return rc;
  }

  // Mark shmem for auto removal when pid exits
  if (shmctl(memory->shmid, IPC_RMID, 0) != 0) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: hugepage memory cannot auto-delete: %s (errno %d)\n",
      strerror(rc), rc);
  }

  // Initialize memory zero
  memset(memory->hugePageMemory, 0, buf_size);

  return 0;
}

int ice_ib_initialize_ipv4_udp_queue(int completionQueueSize, struct ibv_context *context,
  const HugePageMemory *memory) {
  assert(completionQueueSize>0);
  assert(context);
  assert(memory);

  // Huge page memory is to for a IPV4UDPQueue object
  struct IPV4UDPQueue *queue = (struct IPV4UDPQueue *)memory->hugePageMemory;

  // Set extents: [start, end)
  queue->start = (const char *)memory->hugePageMemory;
  queue->end = (const char *)(start + memory->actualSizeBytes);

  char valid = 1;

  // Allocate memory protection domain
  if (0==(queue->pd = ibv_alloc_pd(context))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_initialize_ipv4_udp_queue: ibv_alloc_pd failed: %s (errno %d)\n",
      strerror(rc), rc);
    valid = 0;
  }

  // Allocate memory region with protection
  if (valid && queue->pd) {
    int flags = 0;
    flags |= IBV_ACCESS_LOCAL_READ;
    flags |= IBV_ACCESS_LOCAL_WRITE;
    flags |= IBV_ACCESS_RELAXED_ORDERING;
    queue->mr = ibv_reg_mr(queue->pd, queue->start, memory->actualSizeBytes, flags);
    if (0==queue->mr) {
      int rc = errno;
      fprintf(stderr, "warn : ice_ib_initialize_ipv4_udp_queue: ibv_reg_mr failed: %s (errno %d)\n",
        strerror(rc), rc);
      valid = 0;
  }

  // Allocate a completion queue
  if (0==(queue->cq = ibv_create_cq(context, completionQueueSize, 0, 0, 0))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_initialize_ipv4_udp_queue: ibv_create_cq failed: %s (errno %d)\n",
      strerror(rc), rc);
    valid = 0;
  }

  return valid ? 0 : ICE_IB_ERROR_API_ERROR;
}

int ice_ib_deinitialize_ipv4_udp_queue(HugePageMemory *memory) {
  assert(memory);
}


























































int ice_ib_allocate_session(const struct UserParam *param, const struct ibv_context *context, struct Session *session) {
  assert(param);
  assert(context);
  assert(session);

  // Initialize session
  memset(session, 0, sizeof(struct Session));

  // Start initializing session
  char valid = 1;
  session->userParam = param;
  session->context = context;

  // Get source/dest IPV4 IP addresses into network ready binary format
  if (0==(inet_pton(AF_INET, param->clientIpAddr, &session->srcIpAddr))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: inet_pton failed on '%s': %s (errno %d)\n", param->clientIpAddr,
      strerror(rc), rc);
    valid = 0;
  }
  if (0==(inet_pton(AF_INET, param->serverIpAddr, session->common->dstIpAddr))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: inet_pton failed on '%s': %s (errno %d)\n", param->serverIpAddr,
      strerror(rc), rc);
    valid = 0;
  }

  // Get source/dest IP MAC addreses into network ready binary format
  uint32_t macData[6];
  if (MAC_ADDR_SIZE!=(sscanf(param->clientMac, "%02x:%02x:%02x:%02x:%02x:%02x",
    macData+0, macData+1, macData+2,
    macData+3, macData+4, macData+5))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: MAC address invalid '%s': %s (errno %d)\n",
      param->clientMac, strerror(rc), rc);
    valid = 0;
  }
  for (uint16_t i=0; i<MAC_ADDR_SIZE; ++i) {
    session->common->srcMac[i] = (uint8_t)(macData[i] & 0xff);
  }
  if (MAC_ADDR_SIZE!=(sscanf(param->serverMac, "%02x:%02x:%02x:%02x:%02x:%02x",
    macData+0, macData+1, macData+2,
    macData+3, macData+4, macData+5))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: MAC address invalid '%s': %s (errno %d)\n",
      param->serverMac, strerror(rc), rc);
    valid = 0;
  }
  for (uint16_t i=0; i<MAC_ADDR_SIZE; ++i) {
    session->common->dstMac[i] = (uint8_t)(macData[i] & 0xff);
  }

  // Get source/dest IPV4 IP ports into network ready binary format
  session->common->srcPort = htons(param->clientPort);
  session->common->dstPort = htons(param->serverPort);

  // No point continuing if addresses bad
  if (!valid) {
    return ICE_IB_ERROR_BAD_IP_ADDR;
  }

  // Get memory for send queue
  if (0!=(ice_ib_allocate_huge_memory(sizeof(IPV4UDPQueue), &session->sendMemory))) {
    return ICE_IB_ERROR_NO_MEMORY;
  }
  session->send = (struct IPV4UDPQueue *)(session->sendMemory.hugePageMemory);

  // Get memory for recv queue
  if (0!=(ice_ib_allocate_huge_memory(sizeof(IPV4UDPQueue), &session->recvMemory))) {
    return ICE_IB_ERROR_NO_MEMORY;
  }
  session->recv = (struct IPV4UDPQueue *)(session->recvMemory.hugePageMemory);

  // Get memory for common data
  if (0!=(ice_ib_allocate_huge_memory(sizeof(SessionCommon), &session->cmmnMemory))) {
    return ICE_IB_ERROR_NO_MEMORY;
  }
  session->common = (struct SessionCommon *)(session->cmmnMemory.hugePageMemory);

  // Initialize send queue
  if (0!=(ice_ib_initialize_ipv4_udp_queue(&session->sendMemory);

  // Initialize recv queue
  if (0!=(ice_ib_initialize_ipv4_udp_queue(&session->recvMemory);

  // Initialize data common to send receive
  if (0!=(ice_ib_initialize_session_common(&session->commonMemory);










  if (0==(session->recv_cq = ibv_create_cq((struct ibv_context *)context, param->rxQueueSize, 0, 0, 0))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: ibv_create_cq failed: %s (errno %d)\n",
      strerror(rc), rc);
    valid = 0;
  }

  if (session->send_cq && session->recv_cq && session->pd) {
    struct ibv_qp_init_attr attr;
    struct ibv_qp_init_attr_ex attrEx;
    struct mlx5dv_qp_init_attr attrMlx5;

    memset(&attr, 0, sizeof(attr));
    memset(&attrEx, 0, sizeof(attrEx));
    memset(&attrMlx5, 0, sizeof(attrMlx5));

    attr.send_cq = session->send_cq;
    attr.recv_cq = session->recv_cq;
    attr.cap.max_send_wr = param->txQueueSize;
    attr.cap.max_send_sge = 1;
    attr.cap.max_recv_wr = param->rxQueueSize;
    attr.cap.max_recv_sge = 1;
    attr.qp_type |= IBV_QPT_RAW_PACKET;
    attr.cap.max_inline_data = 0;

    attrEx.pd = session->pd;
    attrEx.send_ops_flags |= IBV_QP_EX_WITH_SEND;
    attrEx.comp_mask |= IBV_QP_INIT_ATTR_SEND_OPS_FLAGS | IBV_QP_INIT_ATTR_PD;
    attrEx.send_cq = session->send_cq;
    attrEx.recv_cq = session->recv_cq;
    attrEx.cap.max_send_wr = attr.cap.max_send_wr;
    attrEx.cap.max_send_sge = attr.cap.max_send_sge;
    attrEx.cap.max_recv_wr  = attr.cap.max_recv_wr;
    attrEx.cap.max_recv_sge = attr.cap.max_recv_sge;
    attrEx.qp_type = attr.qp_type;
    attrEx.srq = attr.srq;
    attrEx.cap.max_inline_data = attr.cap.max_inline_data;

    if (0==(session->qp = ibv_create_qp(session->pd, &attr))) {
      int rc = errno;
      fprintf(stderr, "warn : ice_ib_allocate_session: ibv_create_qp failed: %s (errno %d)\n",
        strerror(rc), rc);
      valid = 0;
    }

    if (session->qp) {
      struct ibv_qp_attr attr;
      memset(&attr, 0, sizeof(attr));
      int flags = IBV_QP_STATE | IBV_QP_PORT;

      attr.qp_state = IBV_QPS_INIT;
      attr.port_num = param->portId;
      
      if (0!=(ibv_modify_qp(session->qp, &attr, flags))) {
        int rc = errno;
        fprintf(stderr, "warn : ice_ib_allocate_session: ibv_modify_qp failed: %s (errno %d)\n",
          strerror(rc), rc);
        valid = 0;
      }
    }
  }


  return valid ? 0 : ICE_IB_ERROR_NO_MEMORY;
}

int ice_ib_deallocate_session(struct Session *session) {
  assert(session);

  if (session->qpExt) {
    free(session->qpExt);
  }
  if (session->devQp) {
    free(session->devQp);
  }
  if (session->mr) {
    free(session->mr);
  }
  if (session->sge) {
    free(session->sge);
  }
  if (session->send_cq) {
    ibv_destroy_cq(session->send_cq);
  }
  if (session->recv_cq) {
    ibv_destroy_cq(session->recv_cq);
  }
  if (session->qp) {
    ibv_destroy_qp(session->qp);
  }
  if (session->mr) {
      ibv_dereg_mr(session->mr);
  }
  if (session->pd) {
    ibv_dealloc_pd(session->pd);
  }

  memset(session, 0, sizeof(struct Session));

  return 0;
}

int ice_ib_set_rtr(struct Session *session) {
  assert(session);

  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(attr));

  int flags = IBV_QP_STATE;

  attr.qp_state = IBV_QPS_RTR;                                                                                         
  attr.ah_attr.src_path_bits = 0;                                                                                      
  attr.ah_attr.port_num = session->userParam->portId;

  int rc = ibv_modify_qp(session->qp, &attr, flags);

  if (rc!=0) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_set_rtr: ibv_modify_qp: failed: %s (errno %d)\n",
      strerror(rc), rc);
  }

  return rc;
}

int ice_ib_set_rts(struct Session *session) {
  assert(session);

  struct ibv_qp_attr attr;
  memset(&attr, 0, sizeof(attr));

  int flags = IBV_QP_STATE;

  attr.qp_state = IBV_QPS_RTS;                                                                                         
  attr.ah_attr.src_path_bits = 0;                                                                                      
  attr.ah_attr.port_num = session->userParam->portId;

  int rc = ibv_modify_qp(session->qp, &attr, flags);

  if (rc!=0) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_set_rtr: ibv_modify_qp: failed: %s (errno %d)\n",
      strerror(rc), rc);
  }

  return rc;
}
