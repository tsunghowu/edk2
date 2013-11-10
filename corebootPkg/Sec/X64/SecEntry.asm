;      TITLE   SecEntry.asm
;------------------------------------------------------------------------------
;*
;*   Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
;*                 2013 Patrick Georgi
;*   This program and the accompanying materials
;*   are licensed and made available under the terms and conditions of the BSD License
;*   which accompanies this distribution.  The full text of the license may be found at
;*   http://opensource.org/licenses/bsd-license.php
;*
;*   THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
;*   WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;*
;*    CpuAsm.asm
;*
;*   Abstract:
;*
;------------------------------------------------------------------------------


;include <Base.h>
;include <Library/PcdLib.h>

;EXTERN ASM_PFX(SecCoreStartupWithStack)

;
; SecCore Entry Point
;
; Processor is in flat protected mode
;
; @return     None  This routine does not return
;
format MS64 COFF
section  'TEXT' align 0x1000 code
align 0x1000
dataStart:

pml4: ; bits 47:39
    dq    pdpt + 3
align 0x1000
pdpt: ; bits 38:29
    dq    pdt + 0x0003
    dq    pdt + 0x1003
    dq    pdt + 0x2003
    dq    pdt + 0x3003
align 0x1000
pdt: ; bits 29:21
    db    0x4000 dup (0)

gdt64:
    dd    0
    dd    0
gdt64_code:
    dw    0
    dw    0
    db    0
    db    0x98
    db    0x20
    db    0
gdt64_data:
    dw    0
    dw    0
    db    0
    db    0x90
    db    0
    db    0
align 4
    dw    0
gdt64_pointer:
    dw    23 ; length
    dq    gdt64

;---------------------------------------------
use32
;---------------------------------------------

readEip:
   mov      eax, [esp]
   ret

public _ModuleEntryPoint
_ModuleEntryPoint:
    call    readEip
    lea     ebx, [eax - $ + dataStart]          ; ebx is dataStart
    mov     ecx, 800h
    lea     edi, [pdt + (ebx - dataStart)]
    mov     eax, 0x83 ; 2MB page, R/W, P
@@:
    mov     [edi], eax
    add     eax, 0x200000
    add     edi, 8
    loop    @b

    ; load new GDT
    lgdt    fword [gdt64_pointer + (ebx - dataStart)]

    ; configure page tables
    lea     eax, [pml4 + (ebx - dataStart)]
    mov     cr3, eax

    ; enable PAE and PSE
    mov     eax, cr4
    bts     eax, 5
    mov     cr4, eax

    ; LM enable
    mov     ecx, 0xc0000080
    rdmsr
    or      eax, 0x100
    wrmsr

    ; enable paging
    mov     eax, cr0
    bts     eax, 31
    mov     cr0, eax

    lea     eax, [realEntry + (ebx - dataStart)]
    push    8
    push    eax
    retf

;---------------------------------------------
use64
;---------------------------------------------

realEntry:

    ;
    ; Load temporary stack top at very low memory.  The C code
    ; can reload to a better address.
    ; Also load the base address of SECFV.
    ;
    mov     rsp, 0x80000
    mov     rbp, 0x800000
    nop

    ;
    ; Setup parameters and call SecCoreStartupWithStack
    ;   rcx: BootFirmwareVolumePtr
    ;   rdx: TopOfCurrentStack
    ;
    mov     rcx, rbp
    mov     rdx, rsp
    sub     rsp, 0x20
    extrn   SecCoreStartupWithStack
    call    SecCoreStartupWithStack
