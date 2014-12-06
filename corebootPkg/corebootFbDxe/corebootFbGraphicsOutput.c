/** @file
Copyright (c) 2007 - 2012, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

Module Name:

  UeficorebootFbGraphicsOutput.c

Abstract:

  This file produces the graphics abstration of Graphics Output Protocol. It is called by
  corebootFb.c file which deals with the EFI 1.1 driver model.
  This file just does graphics.

**/
#include "corebootFb.h"
#include <IndustryStandard/Acpi.h>
#include <Library/BltLib.h>

//
// Graphics Output Protocol Member Functions
//
EFI_STATUS
EFIAPI
corebootFbGraphicsOutputQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
/*++

Routine Description:

  Graphics Output protocol interface to query video mode

  Arguments:
    This                  - Protocol instance pointer.
    ModeNumber            - The mode number to return information on.
    Info                  - Caller allocated buffer that returns information about ModeNumber.
    SizeOfInfo            - A pointer to the size, in bytes, of the Info buffer.

  Returns:
    EFI_SUCCESS           - Mode information returned.
    EFI_BUFFER_TOO_SMALL  - The Info buffer was too small.
    EFI_DEVICE_ERROR      - A hardware error occurred trying to retrieve the video mode.
    EFI_NOT_STARTED       - Video display is not initialized. Call SetMode ()
    EFI_INVALID_PARAMETER - One of the input args was NULL.

--*/
{
  COREBOOT_FB_PRIVATE_DATA  *Private;

  Private = COREBOOT_FB_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS (This);

  if (Info == NULL || SizeOfInfo == NULL || ModeNumber >= This->Mode->MaxMode) {
    return EFI_INVALID_PARAMETER;
  }

  *Info = AllocatePool (sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  #define SIZE_POS_TO_MASK(size, pos) ((1 << (size)) - 1) << (pos)

  (*Info)->Version                       = 0;
  (*Info)->HorizontalResolution          = Private->fbinfo.x_resolution;
  (*Info)->VerticalResolution            = Private->fbinfo.y_resolution;
  (*Info)->PixelFormat                   = PixelBitMask;
  (*Info)->PixelInformation.RedMask      = SIZE_POS_TO_MASK(Private->fbinfo.red_mask_size, Private->fbinfo.red_mask_pos);
  (*Info)->PixelInformation.GreenMask    = SIZE_POS_TO_MASK(Private->fbinfo.green_mask_size, Private->fbinfo.green_mask_pos);
  (*Info)->PixelInformation.BlueMask     = SIZE_POS_TO_MASK(Private->fbinfo.blue_mask_size, Private->fbinfo.blue_mask_pos);
  (*Info)->PixelInformation.ReservedMask = SIZE_POS_TO_MASK(Private->fbinfo.reserved_mask_size, Private->fbinfo.reserved_mask_pos);
  DEBUG ((EFI_D_ERROR, "r mask: %x\n", (*Info)->PixelInformation.RedMask));
  DEBUG ((EFI_D_ERROR, "g mask: %x\n", (*Info)->PixelInformation.GreenMask));
  DEBUG ((EFI_D_ERROR, "b mask: %x\n", (*Info)->PixelInformation.BlueMask));
  DEBUG ((EFI_D_ERROR, "0 mask: %x\n", (*Info)->PixelInformation.ReservedMask));
  (*Info)->PixelsPerScanLine             = (*Info)->HorizontalResolution;

  /* this significantly improves the BitBlt behaviour somehow */
  if ((Private->fbinfo.bits_per_pixel == 32) &&
      ((*Info)->PixelInformation.BlueMask     == 0xff) &&
      ((*Info)->PixelInformation.GreenMask    == 0xff00) &&
      ((*Info)->PixelInformation.RedMask      == 0xff0000) &&
      ((*Info)->PixelInformation.ReservedMask == 0xff000000)) {
    DEBUG ((EFI_D_ERROR, "switching to predefined pixel format: PixelBlueGreenRedReserved8BitPerColor"));
    (*Info)->PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
  }

  DEBUG ((EFI_D_ERROR, "found framebuffer dimensions %dx%d.\n", Private->fbinfo.x_resolution, Private->fbinfo.y_resolution));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
corebootFbGraphicsOutputBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer, OPTIONAL
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION     BltOperation,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height,
  IN  UINTN                                 Delta
  )
/*++

Routine Description:

  Graphics Output protocol instance to block transfer for CirrusLogic device

Arguments:

  This          - Pointer to Graphics Output protocol instance
  BltBuffer     - The data to transfer to screen
  BltOperation  - The operation to perform
  SourceX       - The X coordinate of the source for BltOperation
  SourceY       - The Y coordinate of the source for BltOperation
  DestinationX  - The X coordinate of the destination for BltOperation
  DestinationY  - The Y coordinate of the destination for BltOperation
  Width         - The width of a rectangle in the blt rectangle in pixels
  Height        - The height of a rectangle in the blt rectangle in pixels
  Delta         - Not used for EfiBltVideoFill and EfiBltVideoToVideo operation.
                  If a Delta of 0 is used, the entire BltBuffer will be operated on.
                  If a subrectangle of the BltBuffer is used, then Delta represents
                  the number of bytes in a row of the BltBuffer.

Returns:

  EFI_INVALID_PARAMETER - Invalid parameter passed in
  EFI_SUCCESS - Blt operation success

--*/
{
  EFI_STATUS                      Status;
  EFI_TPL                         OriginalTPL;

  //
  // We have to raise to TPL Notify, so we make an atomic write the frame buffer.
  // We would not want a timer based event (Cursor, ...) to come in while we are
  // doing this operation.
  //
  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);

  switch (BltOperation) {
  case EfiBltVideoToBltBuffer:
  case EfiBltBufferToVideo:
  case EfiBltVideoFill:
  case EfiBltVideoToVideo:
    Status = BltLibGopBlt (
      BltBuffer,
      BltOperation,
      SourceX,
      SourceY,
      DestinationX,
      DestinationY,
      Width,
      Height,
      Delta
      );
    break;

  default:
    Status = EFI_INVALID_PARAMETER;
    ASSERT (FALSE);
  }

  gBS->RestoreTPL (OriginalTPL);

  return Status;
}

EFI_STATUS
corebootFbGraphicsOutputConstructor (
  COREBOOT_FB_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                   Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;


  GraphicsOutput            = &Private->GraphicsOutput;
  GraphicsOutput->QueryMode = corebootFbGraphicsOutputQueryMode;
  GraphicsOutput->Blt       = corebootFbGraphicsOutputBlt;

  //
  // Initialize the private data
  //
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE),
                  (VOID **) &Private->GraphicsOutput.Mode
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                  (VOID **) &Private->GraphicsOutput.Mode->Info
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Private->GraphicsOutput.Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  Private->GraphicsOutput.Mode->FrameBufferBase = Private->fbinfo.physical_address;
  Private->GraphicsOutput.Mode->FrameBufferSize = Private->fbinfo.x_resolution * Private->fbinfo.y_resolution * (Private->fbinfo.bits_per_pixel / 8);
  Private->GraphicsOutput.Mode->MaxMode = 1;
  Private->GraphicsOutput.Mode->Mode    = 0;
  UINTN tmp;
  corebootFbGraphicsOutputQueryMode(GraphicsOutput, 0, &tmp, &Private->GraphicsOutput.Mode->Info);

  BltLibConfigure (
    (VOID*)(UINTN) Private->GraphicsOutput.Mode->FrameBufferBase,
    Private->GraphicsOutput.Mode->Info
    );

  return EFI_SUCCESS;
}

EFI_STATUS
corebootFbGraphicsOutputDestructor (
  COREBOOT_FB_PRIVATE_DATA  *Private
  )
/*++

Routine Description:

Arguments:

Returns:

  None

--*/
{
  if (Private->GraphicsOutput.Mode != NULL) {
    if (Private->GraphicsOutput.Mode->Info != NULL) {
      gBS->FreePool (Private->GraphicsOutput.Mode->Info);
    }
    gBS->FreePool (Private->GraphicsOutput.Mode);
  }

  return EFI_SUCCESS;
}

