/**@file
  Memory Detection for Virtual Machines.

  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

Module Name:

  MemDetect.c

**/

//
// The package level header files this module uses
//
#include <PiPei.h>

//
// The Library classes this module consumes
//
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/ResourcePublicationLib.h>
#include <Library/MtrrLib.h>

#include "Platform.h"
#include "Coreboot.h"

UINT64                      LowerMemorySize;
UINT64                      UpperMemorySize;

extern const EFI_GUID gUefiCorebootPkgFramebufferHob;

/* Helpful inlines */

static UINT64 cb_unpack64(struct cbuint64 val)
{
  return (((UINT64) val.hi) << 32) | val.lo;
}

static const char *cb_mb_vendor_string(const struct cb_mainboard *cbm)
{
  return (char *)(cbm->strings + cbm->vendor_idx);
}

static const char *cb_mb_part_string(const struct cb_mainboard *cbm)
{
  return (char *)(cbm->strings + cbm->part_number_idx);
}


VOID
ParseMemory(
  struct cb_memory *rec
  )
{
  unsigned int i;

  DEBUG ((EFI_D_ERROR, "Found memory information\n"));
  LowerMemorySize = 0;
  UpperMemorySize = 0;

  for (i=0; i< MEM_RANGE_COUNT(rec); i++) {
    struct cb_memory_range *range = MEM_RANGE_PTR(rec, i);
    UINT64 start, size;
    start = cb_unpack64(range->start);
    size = cb_unpack64(range->size);
    DEBUG ((EFI_D_ERROR, "%d. %016lx - %016lx [%02x]\n",
      i, start, start + size - 1, range->type));
    if (range->type != CB_MEM_RAM)
      continue;
    if (start + size < 0x100000000ULL) {
      LowerMemorySize = start + size;
    } else {
      UpperMemorySize = start + size - 0x100000000ULL;
    }
  }
}

VOID
FindTableAt (
  VOID *addr
  )
{
  struct cb_header *header;
  unsigned char *ptr = addr;
  unsigned int i, len=4096;

  for (i = 0; i < len; i += 16, ptr += 16) {
    header = (struct cb_header *)ptr;
    if (header->signature == 0x4f49424c) {
      break;
    }
  }
  
  if (i >= len)
    return;

  if (!header->table_bytes)
    return;

  DEBUG ((EFI_D_ERROR, "Found coreboot table at %p.\n", header));
  ptr += header->header_bytes;
  for (i = 0; i < header->table_entries; i++) {
    struct cb_record *rec = (struct cb_record *)ptr;
    switch (rec->tag) {
    case CB_TAG_FORWARD:
      FindTableAt((void *)(UINTN)((struct cb_forward *)rec)->forward);
      return;
    case CB_TAG_MEMORY:
      ParseMemory((struct cb_memory *)ptr);
      break;
    case CB_TAG_FRAMEBUFFER: {
      void *addr = BuildGuidHob(&gUefiCorebootPkgFramebufferHob, sizeof(struct cb_framebuffer));
      if (addr) {
        CopyMem(addr, ptr, sizeof(struct cb_framebuffer));
      }
    }
    default:
      break;
    }
    ptr += rec->size;
  }
}

VOID
FindCorebootTable (
  VOID
  )
{
  FindTableAt((void *)0);
}

/**
  Peform Memory Detection

  @return EFI_SUCCESS     The PEIM initialized successfully.

**/

EFI_PHYSICAL_ADDRESS
MemDetect (
  )
{
  EFI_STATUS                  Status;
  EFI_PHYSICAL_ADDRESS        MemoryBase;
  UINT64                      MemorySize;

  DEBUG ((EFI_D_ERROR, "MemDetect called\n"));

  //
  // Total memory size available detected by coreboot table
  //
  FindCorebootTable();

  //
  // Determine the range of memory to use during PEI
  //
  MemoryBase = PcdGet32 (PcdCorebootMemFvBase) + PcdGet32 (PcdCorebootMemFvSize);
  MemorySize = LowerMemorySize - MemoryBase;
  if (MemorySize > SIZE_64MB) {
    MemoryBase = LowerMemorySize - SIZE_64MB;
    MemorySize = SIZE_64MB;
  }

  //
  // Publish this memory to the PEI Core
  //
  Status = PublishSystemMemory(MemoryBase, MemorySize);
  ASSERT_EFI_ERROR (Status);

  //
  // Create memory HOBs
  //
  AddMemoryBaseSizeHob (MemoryBase, MemorySize);
  AddMemoryRangeHob (BASE_1MB, MemoryBase);
  AddMemoryRangeHob (0, BASE_512KB + BASE_128KB);

  MtrrSetMemoryAttribute (BASE_1MB, MemoryBase + MemorySize - BASE_1MB, CacheWriteBack);

  MtrrSetMemoryAttribute (0, BASE_512KB + BASE_128KB, CacheWriteBack);

  if (UpperMemorySize != 0) {
    AddUntestedMemoryBaseSizeHob (BASE_4GB, UpperMemorySize);

    MtrrSetMemoryAttribute (BASE_4GB, UpperMemorySize, CacheWriteBack);
  }

  return MemoryBase + MemorySize;
}

