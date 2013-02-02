/** @file

Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2014 Patrick Georgi

This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:
  Smbios.c

Abstract:

Revision History:

**/

#include "AcpiPlatform.h"
#include <Protocol/Smbios.h>
#include <IndustryStandard/SmBios.h>
#include <Guid/SmBios.h>

#define SMBIOS_PTR        SIGNATURE_32('_','S','M','_')

STATIC
SMBIOS_TABLE_ENTRY_POINT *
FindSMBIOSPtr (
  VOID
  )
{
  UINTN                           Address;

  //
  // First Seach 0x0f0000 - 0x0fffff for SMBIOS Ptr
  //
  for (Address = 0xf0000; Address < 0xfffff; Address += 0x10) {
    if (*(UINT32 *)(Address) == SMBIOS_PTR) {
      return (SMBIOS_TABLE_ENTRY_POINT *)Address;
    }
  }
  return NULL;
}

STATIC
EFI_STATUS
RelocateSmbiosTable (
  IN OUT SMBIOS_TABLE_ENTRY_POINT **Table
  )
/*++

Routine Description:

  Relocates Smbios Table if the Location of the SMBios Table is lower than Addres 0x100000

Arguments:
  Table     - pointer to pointer to the table

Returns:
  EFI_SUCEESS - Convert Table successfully
  Other       - Failed

--*/
{
  SMBIOS_TABLE_ENTRY_POINT *SmbiosTableNew;
  SMBIOS_TABLE_ENTRY_POINT *SmbiosTableOri;
  EFI_STATUS               Status;
  UINT32                   SmbiosEntryLen;
  UINT32                   BufferLen;
  EFI_PHYSICAL_ADDRESS     BufferPtr;
  
  SmbiosTableNew  = NULL;
  SmbiosTableOri  = NULL;
  
  //
  // Get Smbios configuration Table 
  //
  SmbiosTableOri = *Table;
  
  if ((SmbiosTableOri == NULL) ||
      ((UINTN)SmbiosTableOri > 0x100000) ||
      ((UINTN)SmbiosTableOri < 0xF0000)){
    return EFI_SUCCESS;
  }
  //
  // Relocate the Smbios memory
  //
  BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS;
  if (SmbiosTableOri->SmbiosBcdRevision != 0x21) {
    SmbiosEntryLen  = SmbiosTableOri->EntryPointLength;
  } else {
    //
    // According to Smbios Spec 2.4, we should set entry point length as 0x1F if version is 2.1
    //
    SmbiosEntryLen = 0x1F;
  }
  BufferLen = SmbiosEntryLen + SYS_TABLE_PAD(SmbiosEntryLen) + SmbiosTableOri->TableLength;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  EfiACPIMemoryNVS,
                  EFI_SIZE_TO_PAGES(BufferLen),
                  &BufferPtr
                  );
  ASSERT_EFI_ERROR (Status);
  SmbiosTableNew = (SMBIOS_TABLE_ENTRY_POINT *)(UINTN)BufferPtr;
  CopyMem (
    SmbiosTableNew, 
    SmbiosTableOri,
    SmbiosEntryLen
    );
  // 
  // Get Smbios Structure table address, and make sure the start address is 32-bit align
  //
  BufferPtr += SmbiosEntryLen + SYS_TABLE_PAD(SmbiosEntryLen);
  CopyMem (
    (VOID *)(UINTN)BufferPtr, 
    (VOID *)(UINTN)(SmbiosTableOri->TableAddress),
    SmbiosTableOri->TableLength
    );
  SmbiosTableNew->TableAddress = (UINT32)BufferPtr;
  SmbiosTableNew->IntermediateChecksum = 0;
  SmbiosTableNew->IntermediateChecksum = 
          CalculateCheckSum8 ((UINT8*)SmbiosTableNew + 0x10, SmbiosEntryLen -0x10);
  //
  // Change the SMBIOS pointer
  //
  *Table = SmbiosTableNew;
  
  return EFI_SUCCESS;  
} 

VOID
AddSmbiosTable()
{
	SMBIOS_TABLE_ENTRY_POINT *Table = FindSMBIOSPtr ();
	if (Table == NULL) return;

	if (RelocateSmbiosTable(&Table) != EFI_SUCCESS) return;

	gBS->InstallConfigurationTable (&gEfiSmbiosTableGuid, (VOID *)Table);
}

