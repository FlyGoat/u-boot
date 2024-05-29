.. SPDX-License-Identifier: GPL-2.0+
.. Copyright (c) 2024 Jiaxun Yang <jiaxun.yang@flygoat.com>

U-Boot Supplement to UEFI Specifications
========================================

Motivation
----------

The UEFI specifications are designed to be platform-independent, enabling the
implementation of UEFI-like API support across various architectures. However,
some platform-dependent aspects and constraints remain. This document provides
a supplement to the UEFI specifications for U-Boot, clarifying these platform
dependent details for architectures not covered by the original specifications.

Architectural Conventions
-------------------------

The UEFI specifications cover IA32, X64, ARM, AARCH64, RISC-V, LoongArch, and
Itanium architectures. This document extends the UEFI specifications to include
all architectures supported by U-Boot.

The following conventions are used for these architectures:

- For architectures with multiple ABIs, we adhere to the calling conventions
  used by the Linux kernel for the same architecture.

- Optional CPU registers (floating-point, SIMD) are excluded from calling
  conventions. However, the UEFI payload should be able to utilize them if
  available.

- Control is handed over to the UEFI payload at the privilege level used
  by the Linux kernel for the same architecture.

- Identity mapping (i.e., VA == PA mapping) is preferred for UEFI. However,
  this may not be possible for some architectures. In such cases, if the
  processor supports a default linear translation, it should be used.

- Endianness: UEFI specifications enforce little-endian architectures.
  However, U-Boot supports both little-endian and big-endian architectures.
  For big-endian architectures, UEFI data structures should be stored in
  native endianness, with exceptions explicitly specified.

UEFI Images
-----------

The UEFI specifications define the PE/COFF image format for UEFI applications.
U-Boot extends this format as follows:

- **PE32+ Machine Type**: UEFI specifications define machine types for supported
  architectures. For machines not covered by UEFI specifications, we use the
  machine type defined by the Microsoft PE/COFF specification if possible.
  Otherwise, we use the ``IMAGE_FILE_MACHINE_UNKNOWN`` (0) machine type.
  U-Boot should always accept ``IMAGE_FILE_MACHINE_UNKNOWN`` as a valid
  machine type.

- **Header Endianness**: PE/COFF header data fields are always stored as
  little-endian, regardless of the target architecture's endianness.

- **DOS Stub**: To accommodate various boot image formats, we relax the requirement
  for a DOS stub in the UEFI image. U-Boot should accept UEFI images without a DOS
  stub and MZ signature. However, U-Boot still expects the PE/COFF header at the
  file offset specified at offset 0x3C.

I/O Device Access
-----------------

UEFI specifications define the EFI_DEVICE_IO_PROTOCOL and EFI_PCI_IO_PROTOCOL
for accessing I/O devices. U-Boot extends these specifications as follows:

- All I/O access is performed using the CPU's native endianness.
  For big-endian architectures, U-Boot should convert data to/from little-endian
  before/after accessing I/O devices.
