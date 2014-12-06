/** @file
  This file produces the graphics abstration of UGA Draw. It is called by
  corebootFb.c file which deals with the EFI 1.1 driver model.
  This file just does graphics.

  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "corebootFb.h"

//
// UGA Draw Protocol Member Functions
//
EFI_STATUS
EFIAPI
corebootFbUgaDrawGetMode (
  IN  EFI_UGA_DRAW_PROTOCOL *This,
  OUT UINT32                *HorizontalResolution,
  OUT UINT32                *VerticalResolution,
  OUT UINT32                *ColorDepth,
  OUT UINT32                *RefreshRate
  )
{
  COREBOOT_FB_PRIVATE_DATA  *Private;

  Private = COREBOOT_FB_PRIVATE_DATA_FROM_UGA_DRAW_THIS (This);

  if ((HorizontalResolution == NULL) ||
      (VerticalResolution == NULL)   ||
      (ColorDepth == NULL)           ||
      (RefreshRate == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *HorizontalResolution = Private->fbinfo.x_resolution;
  *VerticalResolution   = Private->fbinfo.y_resolution;
  *ColorDepth           = Private->fbinfo.bits_per_pixel;
  *RefreshRate          = 60;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
corebootFbUgaDrawSetMode (
  IN  EFI_UGA_DRAW_PROTOCOL *This,
  IN  UINT32                HorizontalResolution,
  IN  UINT32                VerticalResolution,
  IN  UINT32                ColorDepth,
  IN  UINT32                RefreshRate
  )
{
  COREBOOT_FB_PRIVATE_DATA  *Private;

  Private = COREBOOT_FB_PRIVATE_DATA_FROM_UGA_DRAW_THIS (This);

  if ((HorizontalResolution == Private->fbinfo.x_resolution) &&
      (VerticalResolution == Private->fbinfo.y_resolution) &&
      (ColorDepth == Private->fbinfo.bits_per_pixel)) {
      return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

//
// Construction and Destruction functions
//
EFI_STATUS
corebootFbUgaDrawConstructor (
  COREBOOT_FB_PRIVATE_DATA  *Private
  )
{
  EFI_UGA_DRAW_PROTOCOL *UgaDraw;

  //
  // Fill in Private->UgaDraw protocol
  //
  UgaDraw           = &Private->UgaDraw;

  UgaDraw->GetMode  = corebootFbUgaDrawGetMode;
  UgaDraw->SetMode  = corebootFbUgaDrawSetMode;

  return EFI_SUCCESS;
}

