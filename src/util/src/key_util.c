/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2015-2020 Micron Technology, Inc.  All rights reserved.
 */

#include <hse_util/platform.h>
#include <hse_util/minmax.h>
#include <hse_util/key_util.h>
#include <hse/hse_limits.h>

/* If you change the size of struct key_immediate then you'll need to update
 * key_immediate_init(), key_imm_klen(), and key_immediate_cmp_full().
 */
_Static_assert(sizeof(struct key_immediate) == 32,
               "size of key_immediate changed");

/* If HSE_KVS_COUNT_MAX becomes larger than 256 then you'll need
 * to update key_immediate_init() and key_immediate_index().
 */
_Static_assert(HSE_KVS_COUNT_MAX <= 256,
               "HSE_KVS_COUNT_MAX larger than expected");


/* This function may look expensive, but since the size of ki_data[]
 * is known, fixed, and small the optimizer won't generate any calls
 * to memset nor memcpy.
 */
void
key_immediate_init(const void *key, size_t klen, u16 index, struct key_immediate *imm)
{
    size_t dlen = klen;

    if (dlen > KI_DLEN_MAX)
        dlen = KI_DLEN_MAX;

    memset(imm, 0, sizeof(*imm));
    memcpy((char *)imm->ki_data + 1, key, dlen);

    imm->ki_data[0] = be64toh(imm->ki_data[0]);
    imm->ki_data[1] = be64toh(imm->ki_data[1]);
    imm->ki_data[2] = be64toh(imm->ki_data[2]);
    imm->ki_data[3] = be64toh(imm->ki_data[3]);

    imm->ki_data[0] |= (u64)index << 56;
    imm->ki_data[3] |= (dlen << 16) | klen;
}

s32
key_immediate_cmp_full(const struct key_immediate *imm0, const struct key_immediate *imm1)
{
    /* The first comparison includes the skidx.
     */
    if (imm0->ki_data[0] != imm1->ki_data[0])
        return (imm0->ki_data[0] < imm1->ki_data[0]) ? -1 : 1;

    if (imm0->ki_data[1] != imm1->ki_data[1])
        return (imm0->ki_data[1] < imm1->ki_data[1]) ? -1 : 1;

    if (imm0->ki_data[2] != imm1->ki_data[2])
        return (imm0->ki_data[2] < imm1->ki_data[2]) ? -1 : 1;

    /* The final comparison includes the d-length but not the k-length.
     */
    if ((imm0->ki_data[3] >> 16) != (imm1->ki_data[3] >> 16))
        return ((imm0->ki_data[3] >> 16) < (imm1->ki_data[3] >> 16)) ? -1 : 1;

    /* If there is more to compare, tell the caller by returning S32_MIN.
     * Since keys are limited to 1023 bytes at this layer, this can't
     * be a return value from this function other than in this case.
     */
    if (key_imm_klen(imm0) > KI_DLEN_MAX &&
        key_imm_klen(imm1) > KI_DLEN_MAX)
        return S32_MIN;

    /* Otherwise, the result comes down to the key lengths. */
    return (key_imm_klen(imm0) - key_imm_klen(imm1));
}


/* If you change the size of struct key_disc then you'll need
 * to update key_disc_init() and key_disc_cmp().
 */
_Static_assert(sizeof(struct key_disc) == 32,
               "size of key_disc changed");

void
key_disc_init(const void *key, size_t len, struct key_disc *kdisc)
{
    if (len > sizeof(kdisc->kdisc))
        len = sizeof(kdisc->kdisc);

    memset(kdisc, 0, sizeof(*kdisc));
    memcpy(kdisc->kdisc, key, len);

    kdisc->kdisc[0] = be64toh(kdisc->kdisc[0]);
    kdisc->kdisc[1] = be64toh(kdisc->kdisc[1]);
    kdisc->kdisc[2] = be64toh(kdisc->kdisc[2]);
    kdisc->kdisc[3] = be64toh(kdisc->kdisc[3]);
}

int
key_disc_cmp(const struct key_disc *lhs, const struct key_disc *rhs)
{
    if (lhs->kdisc[0] != rhs->kdisc[0])
        return (lhs->kdisc[0] < rhs->kdisc[0]) ? -1 : 1;

    if (lhs->kdisc[1] != rhs->kdisc[1])
        return (lhs->kdisc[1] < rhs->kdisc[1]) ? -1 : 1;

    if (lhs->kdisc[2] != rhs->kdisc[2])
        return (lhs->kdisc[2] < rhs->kdisc[2]) ? -1 : 1;

    if (lhs->kdisc[3] != rhs->kdisc[3])
        return (lhs->kdisc[3] < rhs->kdisc[3]) ? -1 : 1;

    return 0;
}

BullseyeCoverageSaveOff

#if __amd64__
size_t
memlcp(const void *s1, const void *s2, size_t len)
{
    size_t rc;

    /* TODO: Don't directly access rcx...
     */
    __asm__("movq   %1, %0      \n\t" /* rc = len;              */
            "cld                \n\t"
            "movq   %1, %%rcx   \n\t" /* rcx = len;             */
            "jrcxz  1f          \n\t" /* if (rcx == 0) goto 1;  */
            "repz               \n\t" /* while (rcx-- > 0 &&    */
            "cmpsb              \n\t" /*        *s1++ == *s2++) */
            "je     1f          \n\t" /* if (ZF) goto 1;        */
            "subq   %%rcx, %0   \n\t" /* rc -= rcx;             */
            "dec    %0          \n\t" /* rc -= 1;               */
            "1:                 \n\t"
            : "=rax"(rc)
            : "rdx"(len)
            : "rdi", "rsi", "rcx", "memory");

    return rc;
}

size_t
memlcpq(const void *s1, const void *s2, size_t len)
{
    size_t rc;

    /* TODO: Don't directly access rcx...
     */
    __asm__("movq   %1, %0      \n\t" /* rc = len;              */
            "shrq   $3, %0      \n\t" /* rc /= 8;               */
            "cld                \n\t"
            "movq   %0, %%rcx   \n\t" /* rcx = rc;              */
            "jrcxz  1f          \n\t" /* if (rcx == 0) goto 1;  */
            "repz               \n\t" /* while (rcx-- > 0 &&    */
            "cmpsq              \n\t" /*        *s1++ == *s2++) */
            "je     1f          \n\t" /* if (ZF) goto 1;        */
            "subq   %%rcx, %0   \n\t" /* rc -= rcx;             */
            "dec    %0          \n\t" /* rc -= 1;               */
            "1:                 \n\t"
            "shlq   $3, %0      \n\t" /* rc *= 8;               */
            : "=rax"(rc)
            : "rdx"(len)
            : "rdi", "rsi", "rdx", "rcx", "memory");

    return rc;
}

#else
#error memlcp() not implemented for this architecture
#endif

BullseyeCoverageRestore
