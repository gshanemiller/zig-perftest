#pragma once

#include <ib_common.h>

// Return 0 if the Infiniband device 'deviceName' was found and the
// pointer handle '*device' is initialized to point it and non-zero
// otherwise. Behavior is defined provided 'strlen(deviceName)>0'.
int ice_ib_find_device(const char *deviceName, struct ibv_device **device);
