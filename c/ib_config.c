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

int ice_ib_allocate_session(const struct UserParam *param, const struct ibv_context *context, struct SessionParam *session) {
  assert(param);
  assert(context);
  assert(session);

  // Initialize session
  int valid = 1;
  memset(session, 0, sizeof(struct SessionParam));

  // Get source/dest IPV4 IP addresses into network ready binary format
  if (0==(inet_pton(AF_INET, param->serverIpAddr, &session->serverIpAddr))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: inet_pton failed on '%s': %s (errno %d)\n", param->serverIpAddr,
      strerror(rc), rc);
    valid = 0;
  }
  if (0==(inet_pton(AF_INET, param->clientIpAddr, &session->clientIpAddr))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: inet_pton failed on '%s': %s (errno %d)\n", param->clientIpAddr,
      strerror(rc), rc);
    valid = 0;
  }

  // Get source/dest IP MAC addreses into network ready binary format
  uint32_t macData[6];
  if (6!=(sscanf(param->serverMac, "%02x:%02x:%02x:%02x:%02x:%02x",
    macData+0, macData+1, macData+2,
    macData+3, macData+4, macData+5))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: MAC address invalid '%s': %s (errno %d)\n",
      param->serverMac, strerror(rc), rc);
    valid = 0;
  }
  for (uint16_t i=0; i<6; ++i) {
    session->serverMac[i] = (uint8_t)(macData[i] & 0xff);
  }

  // Get source/dest IP MAC addreses into network ready binary format
  if (6!=(sscanf(param->clientMac, "%02x:%02x:%02x:%02x:%02x:%02x",
    macData+0, macData+1, macData+2,
    macData+3, macData+4, macData+5))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: MAC address invalid '%s': %s (errno %d)\n",
      param->clientMac, strerror(rc), rc);
    valid = 0;
  }
  for (uint16_t i=0; i<6; ++i) {
    session->serverMac[i] = (uint8_t)(macData[i] & 0xff);
  }

  // Get source/dest IPV4 IP ports into network ready binary format
  session->serverPort = htons(param->serverPort);
  session->clientPort = htons(param->clientPort);

  // No point continuing if addresses bad
  if (!valid) {
    return ICE_IB_ERROR_BAD_IP_ADDR;
  }

  // Allocate memory
  if (0==(session->qp = (struct ibv_qp *)malloc(sizeof(struct ibv_qp)))) {
    valid = 0;
  }
  
  if (0==(session->qpExt = (struct ibv_qp_ex *)malloc(sizeof(struct ibv_qp_ex)))) {
    valid = 0;
  }

  if (0==(session->devQp = (struct mlx5dv_qp_ex *)malloc(sizeof(struct mlx5dv_qp_ex)))) {
    valid = 0;
  }

  if (0==(session->mr = (struct ibv_mr *)malloc(sizeof(struct ibv_mr)))) {
    valid = 0;
  }

  if (0==(session->sge = (struct ibv_sge *)malloc(sizeof(struct ibv_sge)))) {
    valid = 0;
  }

  if (param->useHugePages) {
    if (0==(session->pd = ibv_alloc_pd((struct ibv_context *)context))) {
      valid = 0;
    }

    session->hugePageAlignSizeBytes = 4096;                                                                                             
    session->needHugePageSizeBytes = 8192; 

    uint64_t buf_size;
    uint64_t alignment = (((session->hugePageAlignSizeBytes +HUGEPAGE_ALIGN_2MB-1)/HUGEPAGE_ALIGN_2MB) * HUGEPAGE_ALIGN_2MB);
    buf_size = (((session->needHugePageSizeBytes + alignment-1 ) / alignment ) * alignment);

    // create huge pages
    session->shmid = shmget(IPC_PRIVATE, buf_size, SHM_HUGETLB | IPC_CREAT | SHM_R | SHM_W);
    if (session->shmid < 0) {
      int rc = errno;
      fprintf(stderr, "warn : ice_ib_allocate_session: cannot allocate %lu bytes of hugepage memory: %s (errno %d)\n",
        buf_size, strerror(rc), rc);
      valid = 0;
    }

    // attach shared memory
    session->hugePageMemory = (void *)shmat(session->shmid, 0, 0);
    if (session->hugePageMemory==(void*)(-1)) {
      int rc = errno;
      fprintf(stderr, "warn : ice_ib_allocate_session: cannot attach %lu bytes of hugepage memory: %s (errno %d)\n",
        buf_size, strerror(rc), rc);
      session->hugePageMemory = 0;
      valid = 0;
    }
    // the next packet will be allocated from here
    session->currentPacket = session->nextPacket = (uint8_t*)(session->hugePageMemory);
    // Make sure it's on a CP cache boundary
    assert(((uint64_t)(session->currentPacket) % CPU_CACHE_LINE_SIZE_BYTES)==0);

    // Mark shmem for auto removal
    if (shmctl(session->shmid, IPC_RMID, 0) != 0) {
      int rc = errno;
      fprintf(stderr, "warn : ice_ib_allocate_session: hugepage memory cannot auto-delete: %s (errno %d)\n",
        strerror(rc), rc);
    }

    // Initialize memory zero
    memset(session->hugePageMemory, 0, buf_size);
    session->hugePageSizeBytes = buf_size;

    int flags = IBV_ACCESS_LOCAL_WRITE;
    flags |= IBV_ACCESS_RELAXED_ORDERING;  // disable_pcir?
 
    if (session->pd && session->hugePageMemory) {
      session->mr = ibv_reg_mr(session->pd, session->hugePageMemory, session->hugePageSizeBytes, flags);
      if (0==session->mr) {
        int rc = errno;
        fprintf(stderr, "warn : ice_ib_allocate_session: ibv_reg_mr failed: %s (errno %d)\n",
          strerror(rc), rc);
        valid = 0;
      }
    }
  }

  if (0==(session->send_cq = ibv_create_cq((struct ibv_context *)context, param->txQueueSize, 0, 0, 0))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_ib_allocate_session: ibv_create_cq failed: %s (errno %d)\n",
      strerror(rc), rc);
    valid = 0;
  }

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

  session->userParam = param;
  session->context = context;

  return valid ? 0 : ICE_IB_ERROR_NO_MEMORY;
}

int ice_ib_deallocate_session(struct SessionParam *session) {
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

  memset(session, 0, sizeof(struct SessionParam));

  return 0;
}

int ice_ib_set_rtr(struct SessionParam *session) {
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

int ice_ib_set_rts(struct SessionParam *session) {
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
