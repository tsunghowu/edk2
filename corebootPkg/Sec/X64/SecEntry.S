#      TITLE   SecEntry.asm
#------------------------------------------------------------------------------
#*
#*   Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
#*                 2013 Patrick Georgi
#*   This program and the accompanying materials
#*   are licensed and made available under the terms and conditions of the BSD License
#*   which accompanies this distribution.  The full text of the license may be found at
#*   http://opensource.org/licenses/bsd-license.php
#*
#*   THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#*   WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#*
#*    CpuAsm.asm
#*
#*   Abstract:
#*
#------------------------------------------------------------------------------


#include <Base.h>
#include <Library/PcdLib.h>

#EXTERN ASM_PFX(SecCoreStartupWithStack)

#
# SecCore Entry Point
#
# Processor is in flat protected mode
#
# @return     None  This routine does not return
#

#define FV_OFFSET 0x64

.data
.align 0x1000
pml4: # bits 47:39
    .quad pdpt + 3
.align 0x1000
pdpt: # bits 38:29
    .quad pdt + 3
.align 0x1000
pdt: # bits 29:21
.space 0x1000, 0

gdt64:
    .long 0
    .long 0
gdt64_code:
    .word 0
    .word 0
    .byte 0
    .byte 0x98
    .byte 0x20
    .byte 0
gdt64_data:
    .word 0
    .word 0
    .byte 0
    .byte 0x90
    .byte 0
    .byte 0
.align 4
    .word 0
gdt64_pointer:
    .word 23 # length
    .quad gdt64

.text
ASM_GLOBAL ASM_PFX(_ModuleEntryPoint)
ASM_PFX(_ModuleEntryPoint):

.code32
    movl    $0x200, %ecx
    movl    $pdt, %edi
    movl    $0x83, %eax # 2MB page, R/W, P
1:
    movl    %eax, (%edi)
    addl    $0x200000, %eax
    addl    $0x8, %edi
    loop    1b

    # load new GDT
    lgdtl   gdt64_pointer

    # configure page tables
    movl    $pml4, %eax
    movl    %eax, %cr3

    # enable PAE and PSE
    movl    %cr4, %eax
    bts     $5, %eax
    movl    %eax, %cr4

    # LM enable
    movl    $0xc0000080, %ecx
    rdmsr
    orl     $0x100, %eax
    wrmsr

    # enable paging
    movl    %cr0, %eax
    bts     $31, %eax
    movl    %eax, %cr0

    ljmpl   $8, $realEntry

.code64
realEntry:

    #
    # Load temporary stack top at very low memory.  The C code
    # can reload to a better address.
    # Also load the base address of SECFV.
    #
    movq    $BASE_512KB, %rsp 
    movq    $FixedPcdGet32(PcdCorebootMemFvBase), %rbp
    nop

    #
    # Setup parameters and call SecCoreStartupWithStack
    #   rcx: BootFirmwareVolumePtr
    #   rdx: TopOfCurrentStack
    #
    movq    %rbp, %rcx
    movq    %rsp, %rdx
    subq    $0x20, %rsp
    call    ASM_PFX(SecCoreStartupWithStack)

