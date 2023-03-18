#include <ib_config.h>

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

  if (0==(session->pd = ibv_alloc_pd(context))) {
    valid = 0;
  }

  param.cycleBuffer = 4096;                                                                                             
  param.cycleBufferSize = 8192; 

  if (param->useHugePages) {
        uint64_t buf_size;
        uint64_t alignment = (((param->cycleBuffer + HUGEPAGE_ALIGN_2MB-1) / HUGEPAGE_ALIGN_2MB) * HUGEPAGE_ALIGN_2MB);
        buf_size = (((param->cycleBufferSize + alignment -1 ) / alignment ) * alignment);

        /* create hugepage shared region */
        ctx->huge_shmid = shmget(IPC_PRIVATE, buf_size,
                                 SHM_HUGETLB | IPC_CREAT | SHM_R | SHM_W);
        if (ctx->huge_shmid < 0) {
                fprintf(stderr, "Failed to allocate hugepages. Please configure hugepages\n");
                return FAILURE;
        }

        /* attach shared memory */
        ctx->buf[qp_index] = (void *) shmat(ctx->huge_shmid, SHMAT_ADDR, SHMAT_FLAGS);
        if (ctx->buf == (void *) -1) {
                fprintf(stderr, "Failed to attach shared memory region\n");
                return FAILURE;
        }

        /* Mark shmem for removal */
        if (shmctl(ctx->huge_shmid, IPC_RMID, 0) != 0) {
                fprintf(stderr, "Failed to mark shm for removal\n");
                return FAILURE;
        }

        return SUCCESS;
  }

  session->userParam = param;
  session->context = context;

  return valid ? 0 : ICE_IB_ERROR_NO_MEMORY;
}

int ice_ib_deallocate_session(struct SessionParam *session) {
  assert(session);

  if (session->qp) {
    free(session->qp);
  }
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
  if (session->pd) {
    ibv_dealloc_pd(session->pd);
  }

  memset(session, 0, sizeof(struct SessionParam));

  return 0;
}

int ice_ib_set_mtu(const struct ibv_context *context, int mtuSizeBytes, int portId) {
  assert(context);
  assert(mtuSizeBytes>0);
  assert(portId>0);
  return 0;
}
