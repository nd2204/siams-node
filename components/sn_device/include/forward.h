#pragma once

#include <stdint.h>

typedef uint16_t local_id_t;

#define INVALID_LOCAL_ID 0
#define LOCAL_ID_MAX     0xff
#define LOCAL_ID_MIN     0x01

#define TO_STR(token) #token
