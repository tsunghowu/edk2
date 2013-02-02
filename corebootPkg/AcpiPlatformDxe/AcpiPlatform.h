/** @file
  Sample ACPI Platform Driver

  Copyright (c) 2008 - 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/ 

#ifndef _ACPI_PLATFORM_H_INCLUDED_
#define _ACPI_PLATFORM_H_INCLUDED_

#include <PiDxe.h>

#include <Protocol/AcpiTable.h>
#include <Protocol/FirmwareVolume2.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

#define EFI_SYSTEM_TABLE_MAX_ADDRESS 0xFFFFFFFF
#define EBDA_BASE_ADDRESS 0x40E
#define SYS_TABLE_PAD(ptr) (((~ptr) +1) & 0x07 )

VOID AddSmbiosTable(VOID);
VOID AddMpsTable(VOID);
VOID AddAcpiTable(VOID);

#endif

