#pragma once
#include <stdint.h>
#include "mcp_forth.h"

int lvgl_forth_run_binary(const uint8_t * binary, int binary_len, const mcp_forth_engine_t * engine);
