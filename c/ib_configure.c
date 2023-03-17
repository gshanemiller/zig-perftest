#pragma

#include <ib_common.h>

// Return 0 if 
ib_config_init_link_layer(const struct ibv_context *context, const struct Param *param)
  assert(context);
  assert(param);

  struct ibv_port_attr port_attr;
  if (ibv_query_port((struct ibv_context *)context, param->ibPort, &port_attr)) {                                                           
    int rc = errno;
    fprintf(stderr, "warn : ibv_query_port: failed: %s (errno %d)\n", strerror(rc), rc);                            
    return (ICE_IB_ERROR_API_ERROR);
  }

  return 0;
}
