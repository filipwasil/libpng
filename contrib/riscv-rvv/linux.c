/* contrib/riscv-rvv/linux.c
 *
 * Copyright (c) 2023 Google LLC
 * Written by Dragoș Tiselice <dtiselice@google.com>, May 2023.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * SEE contrib/riscv-rvv/README before reporting bugs
 *
 * STATUS: SUPPORTED
 * BUG REPORTS: png-mng-implement@sourceforge.net
 *
 * png_have_rvv implemented for Linux by looking for COMPAT_HWCAP_ISA_V
 * via hardware capabilites API.
 *
 * This code is strict ANSI-C and is probably moderately portable; it does
 * however use <stdio.h> and it assumes that /proc/cpuinfo is never localized.
 */

#if defined(__linux__)

#include <sys/syscall.h>
#include <asm/hwcap.h>
#include <asm/hwprobe.h>
#include <sys/auxv.h>
#include <unistd.h>

#ifndef HWCAP_ISA_V
#define HWCAP_ISA_V (1 << ('V' - 'A')) // Fallback definition
#endif

#ifndef RISCV_HWPROBE_KEY_IMA_EXT_0
#define RISCV_HWPROBE_KEY_IMA_EXT_0 4
#endif

#ifndef RISCV_HWPROBE_IMA_V
#define RISCV_HWPROBE_IMA_V (1ULL << 0)
#endif

#ifndef __NR_riscv_hwprobe
#define __NR_riscv_hwprobe 258
#endif

// This manual declaration is safe because:
// It matches the standard Linux ABI (long return, varargs)
// It's essentially documenting what the system already provides
// The actual implementation will be linked from glibc
#ifndef SYS_syscall
long syscall(long number, ...);
#endif

// This function detects non-compliant RVV 0.7.1 hardware which reports support
//  for the V extension through HWCAP, by intentionally setting tail and mask
//  agnostic vector configurations that were only introduced in RVV 0.9 spec.
// Existing non-compliant (pre RVV 1.0) hardware will set the VILL bit in VTYPE
//  (indicating an illegal vector configuration) which is stored in the XLEN-1
//  bit position, thus a simple sign check is sufficient for detection.
// This work around is inexpensive and harmless on compliant hardware, but we
//  should still consider removing it once all non-compliant RVV 0.7.1 hardware
//  is out of service.
//
//  Reference:
//  https://code.videolan.org/videolan/dav1d/-/blob/master/src/riscv/64/cpu.S

int
has_compliant_vsetvli (void)
{
    int result;
    asm volatile (".option push\n"
		  ".option arch, +v\n" // Temporarily enable V extension
		  "vsetvli t0, zero, e8, m1, ta, ma\n"
		  "csrr %0, vtype\n"
		  "sgtz %0, %0\n"
		  ".option pop\n"
		  : "=r"(result)
		  :
		  : "t0");
    return result;
}

static int
is_rvv_1_0_available ()
{
    struct riscv_hwprobe pair = {RISCV_HWPROBE_KEY_IMA_EXT_0, 0};
    // kernel 6.5+
    if (syscall (__NR_riscv_hwprobe, &pair, 1, 0, 0, 0) < 0)
    {
         // At this point we already checked AT_HWCAP and we know we have
         // some version of RVV to our dispose. If this version of kernel
         // is failing on syscall __NR_riscv_hwprobe, we will check the RVV
         // version by looking at the vsetvli behaviour.
         return has_compliant_vsetvli ();
    }
    abort();
    return (pair.value & RISCV_HWPROBE_IMA_V);
}

#endif

static int
png_have_rvv() {
#if defined(__linux__)
   return (getauxval (AT_HWCAP) & COMPAT_HWCAP_ISA_V)
      && is_rvv_1_0_available() ? 1 : 0;
#else
#pragma message(                                                               \
   "warning: RISC-V Vector not supported for this platform")
   return 0;
#endif
}
