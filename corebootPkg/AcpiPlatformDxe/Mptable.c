/*++

Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2014 Patrick Georgi

This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  Mptable.c

Abstract:

--*/

#include "AcpiPlatform.h"
#include <IndustryStandard/LegacyBiosMpTable.h>
#include <Guid/Mps.h>

#define MPS_PTR           SIGNATURE_32('_','M','P','_')

STATIC
EFI_LEGACY_MP_TABLE_FLOATING_POINTER *
FindMPSPtr (
  VOID
  )
{
  UINTN                           Address;
  UINTN                           Index;

  //
  // First Seach 0x0e0000 - 0x0fffff for MPS Ptr
  //
  for (Address = 0xe0000; Address < 0xfffff; Address += 0x10) {
    if (*(UINT32 *)(Address) == MPS_PTR) {
      return (EFI_LEGACY_MP_TABLE_FLOATING_POINTER *)Address;
    }
  }

  //
  // Search EBDA
  //

  Address = (*(UINT16 *)(UINTN)(EBDA_BASE_ADDRESS)) << 4;
  for (Index = 0; Index < 0x400 ; Index += 16) {
    if (*(UINT32 *)(Address + Index) == MPS_PTR) {
      return (EFI_LEGACY_MP_TABLE_FLOATING_POINTER *)Address;
    }
  }
  return NULL;
}

STATIC
EFI_STATUS
RelocateMpsTable (
  IN OUT EFI_LEGACY_MP_TABLE_FLOATING_POINTER **Table
  )
/*++

Routine Description:

  Convert MP Table if the Location of the SMBios Table is lower than Addres 0x100000
  Assumption here:
   As in legacy Bios, MP table is required to place in E/F Seg, 
   So here we just check if the range is E/F seg, 
   and if Not, assume the Memory type is EfiACPIMemoryNVS/EfiRuntimeServicesData
Arguments:
  Table     - pointer to the table

Returns:
  EFI_SUCEESS - Convert Table successfully
  Other       - Failed

--*/
{
  UINT32                                       Data32;
  UINT32                                       FPLength;
  EFI_LEGACY_MP_TABLE_FLOATING_POINTER         *MpsFloatingPointerOri;
  EFI_LEGACY_MP_TABLE_FLOATING_POINTER         *MpsFloatingPointerNew;
  EFI_LEGACY_MP_TABLE_HEADER                   *MpsTableOri;
  EFI_LEGACY_MP_TABLE_HEADER                   *MpsTableNew;
  VOID                                         *OemTableOri;
  VOID                                         *OemTableNew;
  EFI_STATUS                                   Status;
  EFI_PHYSICAL_ADDRESS                         BufferPtr;
  
  //
  // Get MP configuration Table 
  //
  MpsFloatingPointerOri = *Table;
  if (!(((UINTN)MpsFloatingPointerOri <= 0x100000) && 
        ((UINTN)MpsFloatingPointerOri >= 0xF0000))){
    return EFI_SUCCESS;
  }
  //
  // Get Floating pointer structure length
  //
  FPLength = MpsFloatingPointerOri->Length * 16;
  Data32   = FPLength + SYS_TABLE_PAD (FPLength);
  MpsTableOri = (EFI_LEGACY_MP_TABLE_HEADER *)(UINTN)(MpsFloatingPointerOri->PhysicalAddress);
  if (MpsTableOri != NULL) {
    Data32 += MpsTableOri->BaseTableLength;
    Data32 += MpsTableOri->ExtendedTableLength;
    if (MpsTableOri->OemTablePointer != 0x00) {
      Data32 += SYS_TABLE_PAD (Data32);
      Data32 += MpsTableOri->OemTableSize;
    }
  } else {
    return EFI_SUCCESS;
  }
  //
  // Relocate memory
  //
  BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  EfiACPIMemoryNVS,
                  EFI_SIZE_TO_PAGES(Data32),
                  &BufferPtr
                  );
  ASSERT_EFI_ERROR (Status); 
  MpsFloatingPointerNew = (EFI_LEGACY_MP_TABLE_FLOATING_POINTER *)(UINTN)BufferPtr;
  CopyMem (MpsFloatingPointerNew, MpsFloatingPointerOri, FPLength);
  //
  // If Mp Table exists
  //
  if (MpsTableOri != NULL) {
    //
    // Get Mps table length, including Ext table
    //
    BufferPtr = BufferPtr + FPLength + SYS_TABLE_PAD (FPLength);
    MpsTableNew = (EFI_LEGACY_MP_TABLE_HEADER *)(UINTN)BufferPtr;
    CopyMem (MpsTableNew, MpsTableOri, MpsTableOri->BaseTableLength + MpsTableOri->ExtendedTableLength);
    
    if ((MpsTableOri->OemTableSize != 0x0000) && (MpsTableOri->OemTablePointer != 0x0000)){
        BufferPtr += MpsTableOri->BaseTableLength + MpsTableOri->ExtendedTableLength;
        BufferPtr += SYS_TABLE_PAD (BufferPtr);
        OemTableNew = (VOID *)(UINTN)BufferPtr;
        OemTableOri = (VOID *)(UINTN)MpsTableOri->OemTablePointer;
        CopyMem (OemTableNew, OemTableOri, MpsTableOri->OemTableSize);
        MpsTableNew->OemTablePointer = (UINT32)(UINTN)OemTableNew;
    }
    MpsTableNew->Checksum = 0;
    MpsTableNew->Checksum = CalculateCheckSum8 ((UINT8*)MpsTableNew, MpsTableOri->BaseTableLength);
    MpsFloatingPointerNew->PhysicalAddress = (UINT32)(UINTN)MpsTableNew;
    MpsFloatingPointerNew->Checksum = 0;
    MpsFloatingPointerNew->Checksum = CalculateCheckSum8 ((UINT8*)MpsFloatingPointerNew, FPLength);
  }
  //
  // Change the pointer
  //
  *Table = MpsFloatingPointerNew;
  
  return EFI_SUCCESS;  
} 

VOID
AddMpsTable()
{
	EFI_LEGACY_MP_TABLE_FLOATING_POINTER *Table = FindMPSPtr ();
	if (Table == NULL) return;

	if (RelocateMpsTable(&Table) != EFI_SUCCESS) return;

	gBS->InstallConfigurationTable (&gEfiMpsTableGuid, (VOID *)Table);
}

