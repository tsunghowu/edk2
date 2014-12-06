/** @file
  coreboot framebuffer driver
  based on Cirrus Logic 5430 Controller Driver.
  This driver implements UGA Draw and Graphics Output Protocols on top of a
  plain frame buffer prepared by coreboot.
  This driver is only usable in the EFI pre-boot environment.
  This sample is intended to show how the UGA Draw and Graphics output Protocol
  is able to function.
  The UGA I/O Protocol is not implemented in this sample.
  A fully compliant EFI UGA driver requires both
  the UGA Draw and the UGA I/O Protocol.  Please refer to Microsoft's
  documentation on UGA for details on how to write a UGA driver that is able
  to function both in the EFI pre-boot environment and from the OS runtime.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2014 Patrick Georgi
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>

#include "corebootFb.h"

extern const EFI_GUID gUefiCorebootPkgFramebufferHob;

struct cb_framebuffer *fbinfo = NULL;

EFI_DRIVER_BINDING_PROTOCOL gcorebootFbDriverBinding = {
  corebootFbControllerDriverSupported,
  corebootFbControllerDriverStart,
  corebootFbControllerDriverStop,
  0x10,
  NULL,
  NULL
};

/**
  corebootFbControllerDriverSupported

  Check if fbinfo is set (ie we could salvage the data in Pei), and
  if there's a VGA device we can claim.

**/
EFI_STATUS
EFIAPI
corebootFbControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS          Status;
  EFI_PCI_IO_PROTOCOL *PciIo;
  PCI_TYPE00          Pci;

  /* If fbinfo isn't configured, don't bother */
  if (!fbinfo)
    return EFI_UNSUPPORTED;

  //
  // Open the IO Abstraction(s) needed to perform the supported test
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  //
  // See if this is a PCI VGA Controller by looking at the Command register and
  // Class Code Register
  //
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0,
                        sizeof (Pci) / sizeof (UINT32),
                        &Pci
                        );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = EFI_UNSUPPORTED;
  //
  // See if the device is an enabled VGA device.
  // Most systems can only have on VGA device on at a time.
  //
  if (((Pci.Hdr.Command & 0x03) == 0x03) && IS_PCI_VGA (&Pci)) {
    Status = EFI_SUCCESS;
  }

Done:
  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  return Status;
}

/**
  corebootFbControllerDriverStart

  TODO:    This - add argument and description to function comment
  TODO:    Controller - add argument and description to function comment
  TODO:    RemainingDevicePath - add argument and description to function comment
**/
EFI_STATUS
EFIAPI
corebootFbControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS                      Status;
  COREBOOT_FB_PRIVATE_DATA        *Private;
  EFI_DEVICE_PATH_PROTOCOL        *ParentDevicePath;
  ACPI_ADR_DEVICE_PATH            AcpiDeviceNode;

  //
  // Allocate Private context data for UGA Draw inteface.
  //
  Private = AllocateZeroPool (sizeof (COREBOOT_FB_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Error;
  }

  //
  // Set up context record
  //
  Private->Signature  = COREBOOT_FB_PRIVATE_DATA_SIGNATURE;
  Private->Handle     = NULL;
  ASSERT(fbinfo != NULL);
  CopyMem(&Private->fbinfo, fbinfo, sizeof(Private->fbinfo));
  DEBUG(( EFI_D_ERROR, "coreboot fb with dimensions: %dx%d %d bpp\n", fbinfo->x_resolution, fbinfo->y_resolution, fbinfo->bits_per_pixel));
  DEBUG(( EFI_D_ERROR, "coreboot fb at: %x\n", fbinfo->physical_address));

  //
  // Get ParentDevicePath
  //
  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath
                  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  if (FeaturePcdGet (PcdSupportGop)) {
    //
    // Set Gop Device Path
    //
    if (RemainingDevicePath == NULL) {
      ZeroMem (&AcpiDeviceNode, sizeof (ACPI_ADR_DEVICE_PATH));
      AcpiDeviceNode.Header.Type = ACPI_DEVICE_PATH;
      AcpiDeviceNode.Header.SubType = ACPI_ADR_DP;
      AcpiDeviceNode.ADR = ACPI_DISPLAY_ADR (1, 0, 0, 1, 0, ACPI_ADR_DISPLAY_TYPE_VGA, 0, 0);
      SetDevicePathNodeLength (&AcpiDeviceNode.Header, sizeof (ACPI_ADR_DEVICE_PATH));

      Private->GopDevicePath = AppendDevicePathNode (
                                          ParentDevicePath,
                                          (EFI_DEVICE_PATH_PROTOCOL *) &AcpiDeviceNode
                                          );
    } else if (!IsDevicePathEnd (RemainingDevicePath)) {
      //
      // If RemainingDevicePath isn't the End of Device Path Node, 
      // only scan the specified device by RemainingDevicePath
      //
      Private->GopDevicePath = AppendDevicePathNode (ParentDevicePath, RemainingDevicePath);
    } else {
      //
      // If RemainingDevicePath is the End of Device Path Node, 
      // don't create child device and return EFI_SUCCESS
      //
      Private->GopDevicePath = NULL;
    }
      
    if (Private->GopDevicePath != NULL) {
      //
      // Creat child handle and device path protocol firstly
      //
      Private->Handle = NULL;
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Private->Handle,
                      &gEfiDevicePathProtocolGuid,
                      Private->GopDevicePath,
                      NULL
                      );
    }
  }

  if (FeaturePcdGet (PcdSupportUga)) {
    //
    // Start the UGA Draw software stack.
    //
    Status = corebootFbUgaDrawConstructor (Private);
    ASSERT_EFI_ERROR (Status);

    Private->UgaDevicePath = ParentDevicePath;
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Controller,
                    &gEfiUgaDrawProtocolGuid,
                    &Private->UgaDraw,
                    &gEfiDevicePathProtocolGuid,
                    Private->UgaDevicePath,
                    NULL
                    );

  }
  if (FeaturePcdGet (PcdSupportGop)) {
    if (Private->GopDevicePath == NULL) {
      //
      // If RemainingDevicePath is the End of Device Path Node, 
      // don't create child device and return EFI_SUCCESS
      //
      Status = EFI_SUCCESS;
    } else {
  
      //
      // Start the GOP software stack.
      //
      Status = corebootFbGraphicsOutputConstructor (Private);
      ASSERT_EFI_ERROR (Status);
  
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Private->Handle,
                      &gEfiGraphicsOutputProtocolGuid,
                      &Private->GraphicsOutput,
                      NULL
                      );
    }
  } else if (!FeaturePcdGet (PcdSupportUga)) {
    //
    // This driver must support eithor GOP or UGA or both.
    //
    ASSERT (FALSE);
    Status = EFI_UNSUPPORTED;
  }


Error:
  if (EFI_ERROR (Status)) {
    if (Private) {
      gBS->FreePool (Private);
    }
  }

  return Status;
}

/**
  corebootFbControllerDriverStop

  TODO:    This - add argument and description to function comment
  TODO:    Controller - add argument and description to function comment
  TODO:    NumberOfChildren - add argument and description to function comment
  TODO:    ChildHandleBuffer - add argument and description to function comment
  TODO:    EFI_SUCCESS - add return value to function comment
**/
EFI_STATUS
EFIAPI
corebootFbControllerDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN UINTN                          NumberOfChildren,
  IN EFI_HANDLE                     *ChildHandleBuffer
  )
{
  EFI_UGA_DRAW_PROTOCOL           *UgaDraw;
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *GraphicsOutput;

  EFI_STATUS                      Status;
  COREBOOT_FB_PRIVATE_DATA  *Private;

  if (FeaturePcdGet (PcdSupportUga)) {
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiUgaDrawProtocolGuid,
                    (VOID **) &UgaDraw,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
    //
    // Get our private context information
    //
    Private = COREBOOT_FB_PRIVATE_DATA_FROM_UGA_DRAW_THIS (UgaDraw);
    corebootFbUgaDrawDestructor (Private);

    if (FeaturePcdGet (PcdSupportGop)) {
      corebootFbGraphicsOutputDestructor (Private);
      //
      // Remove the UGA and GOP protocol interface from the system
      //
      Status = gBS->UninstallMultipleProtocolInterfaces (
                      Private->Handle,
                      &gEfiUgaDrawProtocolGuid,
                      &Private->UgaDraw,
                      &gEfiGraphicsOutputProtocolGuid,
                      &Private->GraphicsOutput,
                      NULL
                      );
    } else {
      //
      // Remove the UGA Draw interface from the system
      //
      Status = gBS->UninstallMultipleProtocolInterfaces (
                      Private->Handle,
                      &gEfiUgaDrawProtocolGuid,
                      &Private->UgaDraw,
                      NULL
                      );
    }
  } else {
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **) &GraphicsOutput,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    //
    // Get our private context information
    //
    Private = COREBOOT_FB_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS (GraphicsOutput);

    corebootFbGraphicsOutputDestructor (Private);
    //
    // Remove the GOP protocol interface from the system
    //
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    Private->Handle,
                    &gEfiUgaDrawProtocolGuid,
                    &Private->UgaDraw,
                    &gEfiGraphicsOutputProtocolGuid,
                    &Private->GraphicsOutput,
                    NULL
                    );
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Free our instance data
  //
  gBS->FreePool (Private);

  return EFI_SUCCESS;
}

/**
  corebootFbUgaDrawDestructor

  TODO:    Private - add argument and description to function comment
  TODO:    EFI_SUCCESS - add return value to function comment
**/
EFI_STATUS
corebootFbUgaDrawDestructor (
  COREBOOT_FB_PRIVATE_DATA  *Private
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
InitializecorebootFb (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS              Status;

  void *Hob = GetFirstGuidHob(&gUefiCorebootPkgFramebufferHob);
  if (!Hob)
    return EFI_UNSUPPORTED;
  if (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof(struct cb_framebuffer))
    return EFI_UNSUPPORTED;

  fbinfo = GET_GUID_HOB_DATA (Hob);

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gcorebootFbDriverBinding,
             ImageHandle,
             &gcorebootFbComponentName,
             &gcorebootFbComponentName2
             );
  ASSERT_EFI_ERROR (Status);

  //
  // Install EFI Driver Supported EFI Version Protocol required for
  // EFI drivers that are on PCI and other plug in cards.
  //
  gcorebootFbDriverSupportedEfiVersion.FirmwareVersion = PcdGet32 (PcdDriverSupportedEfiVersion);
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDriverSupportedEfiVersionProtocolGuid,
                  &gcorebootFbDriverSupportedEfiVersion,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
