#pragma once

#include <ib.h>
#include <umad.h>
#include <verbs.h>
#include <mlx5dv.h>
#include <mlx5_api.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

// define to not conflict with errno
enum ICE_IB_Error {
  ICE_IB_ERROR_NO_DEVICE = -1,        // Zero IB devices found
  ICE_IB_ERROR_ENOENT_DEVICE = -2,    // Requested IB device not known
  ICE_IB_ERROR_NO_MEMORY = -3,        // Memory allocation failed
  ICE_IB_ERROR_API_ERROR = -4,        // ib/mlx5 api error not enumerated elsewhere
  ICE_IB_ERROR_MAX = -5,
};
