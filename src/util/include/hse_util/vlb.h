/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2015-2020 Micron Technology, Inc.  All rights reserved.
 */

#ifndef HSE_PLATFORM_VLB_H
#define HSE_PLATFORM_VLB_H

#include <hse_util/hse_err.h>

#include <hse/hse_limits.h>

/* The very-large-buffer cache maintains a small pool of large, page
 * aligned buffers for use cases such as direct reads and compression.
 * The cache helps to minimize the number of trips into the kernel
 * that would otherwise perform expensive address map modifications.
 *
 * The vlb cache makes an effort to return a NUMA local buffer to
 * callers of vlb_alloc(), but more work needs to be done to improve
 * the situation.
 */
#define VLB_ALLOCSZ_MAX     (HSE_KVS_VLEN_MAX * 2)
#define VLB_CACHESZ_MAX     (2ul << 30)
#define VLB_KEEPSZ_MAX      (1ul << 20)

/**
 * vlb_alloc() - allocate a read buffer
 * @sz: requested buffer size
 *
 * Caller may request any size, but only requests of size
 * VLB_ALLOCSZ_MAX or smaller will come from the cache.
 * Requests larger than VLB_ALLOCSZ_MAX must specify the
 * same size parameter to vlb_free() as given to vlb_alloc().
 */
void *vlb_alloc(size_t sz);

/**
 * vlb_free() - free a read buffer
 * @mem:  buffer address from vlb_alloc()
 * @used: see below
 *
 * %used must be the exact size of the allocation given to vlb_alloc()
 * if it was larger than VLB_ALLOCSZ_MAX.  Otherwise, it should be
 * a best effort estimate of how much of the buffer was actually
 * modified.
 */
void vlb_free(void *mem, size_t used);

merr_t vlb_init(void) __cold;
void vlb_fini(void) __cold;

#endif
