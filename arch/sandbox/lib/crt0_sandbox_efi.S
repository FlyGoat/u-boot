/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * PE/COFF header for EFI applications
 *
 * Copyright (c) 2019 Heinrich Schuchardt
 */

#include <config.h>

#if CONFIG_IS_ENABLED(HOST_ARCH_X86_64)
#include "../../../arch/x86/lib/crt0_x86_64_efi.S"
#endif

#if CONFIG_IS_ENABLED(HOST_ARCH_X86)
#include "../../../arch/x86/lib/crt0_ia32_efi.S"
#endif

#if CONFIG_IS_ENABLED(HOST_ARCH_AARCH64)
#include "../../../arch/arm/lib/crt0_aarch64_efi.S"
#endif

#if CONFIG_IS_ENABLED(HOST_ARCH_ARM)
#include "../../../arch/arm/lib/crt0_arm_efi.S"
#endif

#if CONFIG_IS_ENABLED(HOST_ARCH_RISCV32)
#include "../../../arch/riscv/lib/crt0_riscv_efi.S"
#endif

#if CONFIG_IS_ENABLED(HOST_ARCH_RISCV64)
#include "../../../arch/riscv/lib/crt0_riscv_efi.S"
#endif
