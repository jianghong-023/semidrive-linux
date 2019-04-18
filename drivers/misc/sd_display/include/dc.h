/*
 * Copyright (c) Semidrive
 */
#pragma once

#include <linux/types.h>

typedef struct dc_profile_t
{
	uint8_t  id;
	ulong   base_addr;
	uint32_t irq;
	uint32_t tcon_irq;
	uint32_t frame_w;
	uint32_t frame_h;
	uint32_t layer_n;
} dc_profile_t;

/* register ops */
inline static ulong reg_addr(ulong base, uint32_t offset)
{
  return (base + offset);
}

inline static uint32_t reg_value(uint32_t val, uint32_t src, uint32_t shift, uint32_t mask)
{
  return (src & ~mask) | ((val << shift) & mask);
}
