/** @file

Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:
  LegacyTable.c

Abstract:

Revision History:

**/

#include "AcpiPlatform.h"
#include <IndustryStandard/Acpi.h>
#include <Guid/Acpi.h>

STATIC
VOID *
FindAcpiRsdPtr (
  VOID
  )
{
  UINTN                           Address;
  UINTN                           Index;

  //
  // First Seach 0x0e0000 - 0x0fffff for RSD Ptr
  //
  for (Address = 0xe0000; Address < 0xfffff; Address += 0x10) {
    if (*(UINT64 *)(Address) == EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE) {
      return (VOID *)Address;
    }
  }

  //
  // Search EBDA
  //

  Address = (*(UINT16 *)(UINTN)(EBDA_BASE_ADDRESS)) << 4;
  for (Index = 0; Index < 0x400 ; Index += 16) {
    if (*(UINT64 *)(Address + Index) == EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE) {
      return (VOID *)Address;
    }
  }
  return NULL;
}

STATIC
EFI_STATUS
RelocateAcpiTable (
  IN OUT VOID **Table
  )
/*++

Routine Description:
  Convert RSDP of ACPI Table if its location is lower than Address:0x100000
  Assumption here:
   As in legacy Bios, ACPI table is required to place in E/F Seg, 
   So here we just check if the range is E/F seg, 
   and if Not, assume the Memory type is EfiACPIReclaimMemory/EfiACPIMemoryNVS

Arguments:
  TableLen  - Acpi RSDP length
  Table     - pointer to the table  

Returns:
  EFI_SUCEESS - Convert Table successfully
  Other       - Failed

--*/
{
  VOID                  *AcpiTableOri;
  VOID                  *AcpiTableNew;
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  BufferPtr;
  UINTN                 TableLen;

  if (((EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)Table)->Reserved == 0x00){
    //
    // If Acpi 1.0 Table, then RSDP structure doesn't contain Length field, use structure size
    //
    TableLen = sizeof (EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER);
  } else if (((EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)Table)->Reserved >= 0x02){
    //
    // If Acpi 2.0 or later, use RSDP Length fied.
    //
    TableLen = ((EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)Table)->Length;
  } else {
    //
    // Invalid Acpi Version, return
    //
    return EFI_UNSUPPORTED;
  }
  
  AcpiTableOri    =  *Table;
  if (((UINTN)AcpiTableOri < 0x100000) && ((UINTN)AcpiTableOri > 0xE0000)) {
    BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS;
    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    EfiACPIMemoryNVS,
                    EFI_SIZE_TO_PAGES(TableLen),
                    &BufferPtr
                    );
    ASSERT_EFI_ERROR (Status);
    AcpiTableNew = (VOID *)(UINTN)BufferPtr;
    CopyMem (AcpiTableNew, AcpiTableOri, TableLen);
  } else {
    AcpiTableNew = AcpiTableOri;
  }
  //
  // Change configuration table Pointer
  //
  *Table = AcpiTableNew;
  
  return EFI_SUCCESS;
}

VOID
AddAcpiTable()
{
	VOID *Table = FindAcpiRsdPtr ();
	if (Table == NULL) return;

	if (RelocateAcpiTable(&Table) != EFI_SUCCESS) return;

	gBS->InstallConfigurationTable (&gEfiAcpi20TableGuid, Table);
	gBS->InstallConfigurationTable (&gEfiAcpiTableGuid, Table);
}

