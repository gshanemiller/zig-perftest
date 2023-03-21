#include <ib_config.h>

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int ice_ib_config_check_port_device(struct ibv_context *context, int portId) {
  assert(context);
  assert(portId>0);

  struct ibv_port_attr attr;
  if (ibv_query_port(context, portId, &attr)) {
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

struct ibv_device **ice_ib_find_device(const char *deviceName, struct ibv_device **device, int *rc) {
  assert(deviceName);
  assert(strlen(deviceName)>0);
  assert(device);
  assert(rc);

  // Initialize
  *device = 0;
  *rc = ICE_IB_ERROR_NO_DEVICE;

  // Get device list and count
  int deviceCount = -1;
  struct ibv_device **list = ibv_get_device_list(&deviceCount);

  if (deviceCount<=0 || list==0) {
    int rc = errno;
    fprintf(stderr, "warn : ibv_get_device_list failed: %s (errno %d)\n", strerror(rc), rc);
    return 0;
  }

  // See if 'deviceName' is found in list
  for (struct ibv_device **item = list; *item; ++item) {
    if (!strcmp(ibv_get_device_name(*item), deviceName)) {
      // matched device
      *device = *item;
      rc = 0;
      break;
    }
  }

  // OK there are devices but none matching 'deviceName'
  if (rc!=0) {
    *rc = ICE_IB_ERROR_ENOENT_DEVICE;
  }

  return list;
}

int ice_ib_allocate_huge_memory(uint64_t requestSizeBytes, struct HugePageMemory *memory) {
  assert(requestSizeBytes>0);
  assert(memory!=0);
  
  // Round request size up to nearest 2MB
  uint64_t buf_size = requestSizeBytes;
  if (buf_size<HUGEPAGE_ALIGN_2MB) {
    buf_size = HUGEPAGE_ALIGN_2MB;
  } else if ((buf_size%HUGEPAGE_ALIGN_2MB)!=0) {
    uint64_t buf_remainder = buf_size & (HUGEPAGE_ALIGN_2MB-1);
    buf_size += (HUGEPAGE_ALIGN_2MB-buf_remainder);
  }
  assert((buf_size%HUGEPAGE_ALIGN_2MB)==0);

  // Record request v. actual memory ask
  memory->actualSizeBytes = buf_size;
  memory->requestSizeBytes = requestSizeBytes;

  fprintf(stderr, "info : ice_ib_allocate_huge_memory: requested %lu bytes rounded up to %lu bytes\n",
    requestSizeBytes, buf_size);
    
  // create 2MB huge pages with read/write permissions
  memory->shmid = shmget(IPC_PRIVATE, buf_size, SHM_HUGETLB | IPC_CREAT | SHM_R | SHM_W);
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
  memset((void*)memory->hugePageMemory, 0, buf_size);

  return 0;
}

int ice_ib_initialize_queue(struct ibv_pd *pd, struct ibv_context *context, struct HugePageMemory *memory) {
  assert(pd);
  assert(context);
  assert(memory);

  // This huge page memory is for a Queue object
  struct Queue *queue = (struct Queue *)memory->hugePageMemory;

  // Set extents: [start, end)
  queue->start = (const uint8_t *)memory->hugePageMemory;
  queue->end = (const uint8_t *)(queue->start + memory->actualSizeBytes);
  assert((queue->end-queue->start)==memory->actualSizeBytes);

  // Assume will succeed
  char valid = 1;

  // Allocate memory region with protection
  if (valid && pd) {
    int flags = 0;
    flags |= IBV_ACCESS_LOCAL_WRITE;
    flags |= IBV_ACCESS_RELAXED_ORDERING;
    queue->mr = ibv_reg_mr(pd, (void*)queue->start, memory->actualSizeBytes, flags);
    if (0==queue->mr) {
      int rc = errno;
      fprintf(stderr, "warn : ice_ib_initialize_queue: ibv_reg_mr failed: %s (errno %d)\n",
        strerror(rc), rc);
      valid = 0;
    }
  }

  // Allocate a completion queue
  if (0==(queue->cq = ibv_create_cq(context, MAX_COMPLETION_QUEUE_ENTRIES, 0, 0, 0))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_initialize_queue: ibv_create_cq failed: %s (errno %d)\n",
      strerror(rc), rc);
    valid = 0;
  }

  return valid ? 0 : ICE_IB_ERROR_API_ERROR;
}

int ice_ib_deinitialize_queue(struct Queue *queue) {
  if (queue->cq) {
    ibv_destroy_cq(queue->cq);
  }
  if (queue->mr) {
    ibv_dereg_mr(queue->mr);
  }

  memset(queue, 0, sizeof(struct Queue));

  return 0;
}

int ice_ib_initialize_session_common(const struct UserParam *param, struct Queue *send, struct Queue *recv,
  struct ibv_context *context, struct ibv_pd *pd, struct ibv_device *device, struct ibv_device **deviceList,
  struct HugePageMemory *memory) {
  assert(param);
  assert(send);
  assert(recv);
  assert(context);
  assert(pd);
  assert(device);
  assert(deviceList);
  assert(memory);

  // This huge page memory is for a SessionCommon object
  struct SessionCommon *common = (struct SessionCommon *)memory->hugePageMemory;

  // Save simple state
  common->pd = pd;
  common->context = context;
  common->device = device;
  common->deviceList = deviceList;

  // Setup qp
  char valid = 1;
  struct ibv_qp_init_attr attr;
  memset(&attr, 0, sizeof(attr));

  attr.send_cq = send->cq;
  attr.recv_cq = recv->cq;
  attr.cap.max_send_wr = MAX_SEND_QUEUE_ENTRIES;
  attr.cap.max_send_sge = 1;
  attr.cap.max_recv_wr = MAX_RECV_QUEUE_ENTRIES;
  attr.cap.max_recv_sge = 1;
  attr.qp_type |= IBV_QPT_RAW_PACKET;
  attr.cap.max_inline_data = 0;

  if (0==(common->qp = ibv_create_qp(pd, &attr))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_initialize_session_common: ibv_create_qp failed: %s (errno %d)\n",
      strerror(rc), rc);
    valid = 0;
  }

  // Put qp into init state
  if (common->qp) {
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));
    int flags = IBV_QP_STATE | IBV_QP_PORT;

    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = param->portId;
      
    if (0!=(ibv_modify_qp(common->qp, &attr, flags))) {
      int rc = errno;
      fprintf(stderr, "warn : :ice_ib_initialize_session_common: ibv_modify_qp failed: %s (errno %d)\n",
        strerror(rc), rc);
      valid = 0;
    }
  }

  // Get source/dest IPV4 IP addresses into network ready binary format
  if (0==(inet_pton(AF_INET, param->clientIpAddr, &common->srcIpAddr))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_initialize_session_common: inet_pton failed on '%s': %s (errno %d)\n", param->clientIpAddr,
      strerror(rc), rc);
    valid = 0;
  }
  if (0==(inet_pton(AF_INET, param->serverIpAddr, &common->dstIpAddr))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_initialize_session_common: inet_pton failed on '%s': %s (errno %d)\n", param->serverIpAddr,
      strerror(rc), rc);
    valid = 0;
  }

  // Get source/dest IP MAC addreses into network ready binary format
  uint32_t macData[6];
  if (MAC_ADDR_SIZE!=(sscanf(param->clientMac, "%02x:%02x:%02x:%02x:%02x:%02x",
    macData+0, macData+1, macData+2,
    macData+3, macData+4, macData+5))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_initialize_session_common: MAC address invalid '%s': %s (errno %d)\n",
      param->clientMac, strerror(rc), rc);
    valid = 0;
  }
  for (uint16_t i=0; i<MAC_ADDR_SIZE; ++i) {
    common->srcMac[i] = (uint8_t)(macData[i] & 0xff);
  }
  if (MAC_ADDR_SIZE!=(sscanf(param->serverMac, "%02x:%02x:%02x:%02x:%02x:%02x",
    macData+0, macData+1, macData+2,
    macData+3, macData+4, macData+5))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_initialize_session_common: MAC address invalid '%s': %s (errno %d)\n",
      param->serverMac, strerror(rc), rc);
    valid = 0;
  }
  for (uint16_t i=0; i<MAC_ADDR_SIZE; ++i) {
    common->dstMac[i] = (uint8_t)(macData[i] & 0xff);
  }

  // Get source/dest IPV4 IP ports into network ready binary format
  common->srcPort = htons(param->clientPort);
  common->dstPort = htons(param->serverPort);

  return valid ? 0 : ICE_IB_ERROR_API_ERROR;
}

int ice_ib_deinitalize_session_common(struct SessionCommon *common) {
  assert(common);

  if (common->qp) {
    ibv_destroy_qp(common->qp);
  }
  if (common->pd) {
    ibv_dealloc_pd(common->pd);
  }
  if (common->context) {
    ibv_close_device(common->context);
  }
  if (common->deviceList) {
    ibv_free_device_list(common->deviceList);
  }

  memset(common, 0, sizeof(struct SessionCommon));

  return 0;
}

int ice_ib_allocate_session(const struct UserParam *param, struct Session *session) {
  assert(param);
  assert(session);

  // Initialize session
  memset(session, 0, sizeof(struct Session));

  // See if the user's device exists
  int rc;
  struct ibv_device *device;
  struct ibv_device **deviceList;
  deviceList = ice_ib_find_device(param->deviceId, &device, &rc);

  // Open a context on found device, if any
  struct ibv_context *context = 0;
  if (device) {
    context = ibv_open_device(device);
    if (context==0) {
      rc = errno;
      fprintf(stderr, "warn : ice_ib_allocate_session: ibv_open_device failed: %s (errno %d)\n", strerror(rc), rc);
      // No viable device
      return ICE_IB_ERROR_NO_DEVICE;
    }
  }

  if (context) {
    if (0!=(ice_ib_config_check_port_device(context, param->portId))) {
      // No viable device
      return ICE_IB_ERROR_NO_DEVICE;
    }
  }

  // Start initializing session
  char valid = 1;
  session->userParam = param;

  // Allocate memory protection domain
  struct ibv_pd *pd = 0;
  if (0==(pd = ibv_alloc_pd(context))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: ibv_alloc_pd failed: %s (errno %d)\n",
      strerror(rc), rc);
    return ICE_IB_ERROR_API_ERROR;
  }

  // Allocate memory for send queue
  if (0==ice_ib_allocate_huge_memory(sizeof(struct Queue), &session->sendMemory)) {
    session->send = (struct Queue *)session->sendMemory.hugePageMemory;
  } else {
    valid = 0;
  }

  // Allocate memory for recv queue
  if (0==ice_ib_allocate_huge_memory(sizeof(struct Queue), &session->recvMemory)) {
    session->recv = (struct Queue *)session->recvMemory.hugePageMemory;
  } else {
    valid = 0;
  }

  // Allocate memory for common data
  if (0==ice_ib_allocate_huge_memory(sizeof(struct SessionCommon), &session->cmmnMemory)) {
    session->common = (struct SessionCommon *)session->cmmnMemory.hugePageMemory;
  } else {
    valid = 0;
  }

  // Initialize send queue
  if (session->send) {
    if (0!=(ice_ib_initialize_queue(pd, context, &session->sendMemory))) {
      valid = 0;
    }
  }

  // Initialize recv queue
  if (session->recv) {
    if (0!=(ice_ib_initialize_queue(pd, context, &session->recvMemory))) {
      valid = 0;
    }
  }
  
  // Initialize data common to send/receive
  if (session->common) {
    if (0!=(ice_ib_initialize_session_common(param, session->send, session->recv, context, pd, device, deviceList,
      &session->cmmnMemory))) {
      valid = 0;
    }
  }

  return valid ? 0 : ICE_IB_ERROR_API_ERROR;
}

int ice_ib_deallocate_session(struct Session *session) {
  assert(session);

  if (session->send) {
    ice_ib_deinitialize_queue(session->send);
  }
  if (session->recv) {
    ice_ib_deinitialize_queue(session->recv);
  }
  if (session->common) {
    ice_ib_deinitalize_session_common(session->common);
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

  int rc = ibv_modify_qp(session->common->qp, &attr, flags);

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

  int rc = ibv_modify_qp(session->common->qp, &attr, flags);

  if (rc!=0) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_set_rtr: ibv_modify_qp: failed: %s (errno %d)\n",
      strerror(rc), rc);
  }

  return rc;
}
