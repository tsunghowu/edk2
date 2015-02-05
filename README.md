Tianocore as coreboot payload
=============================

This branch introduces the corebootPkg. It allows to easily build a
tianocore image that is suitable as coreboot payload.

First you need to set up your EDK2 environment. You can try to use your
host gcc for which edk2 provides toolchain configurations GCC44 through
GCC49. You also need iasl and nasm installed.

You will also need the EDK2 base tools, which can be generated through

    (cd BaseTools/Source/C; make)

After entering the environment (". edksetup.sh"), build corebootPkg
using one of

    build -t YOURTOOLCHAIN -a IA32 -p corebootPkg/corebootPkg.dsc
    build -t YOURTOOLCHAIN -a X64 -p corebootPkg/corebootPkg.dsc

This ideally creates Build/coreboot$ARCH/DEBUG_YOURTOOLCHAIN/FV/COREBOOT.fd,
the payload image. Use -b RELEASE for release builds and -b NOOPT for
no-optimization builds, which also end up in their respective paths.

Configure coreboot to use a Tianocore payload and point it to the
COREBOOT.fd created before.

Build, run, have fun.  If (no, when) you find bugs, I'd love to see your
patches that fix them :-)

CSM Support
-----------
It's possible to run corebootPkg with SeaBIOS as CSM. David Woodhouse
did the necessary work at http://git.infradead.org/users/dwmw2/edk2.git
which I integrated (Thanks David!).

1. Fetch SeaBIOS master (eg. git clone http://review.coreboot.org/seabios.git).
2. Build SeaBIOS as CSM (General Features - Build Target -
   Build as Compatibility Support Module ...)
3. Copy $SEABIOS/out/Csm16.bin to $TIANO/corebootPkg/Csm/Csm16/Csm16.bin
4. Add -D CSM_ENABLE=TRUE to the tiano build line above

From here, proceed as usual (add COREBOOT.fd as payload).

Please note that the vgabios-cirrus.bin shipped with QEmu/KVM isn't
exactly in the format CSM expects.
See https://lists.gnu.org/archive/html/qemu-devel/2013-01/msg03650.html

Using SeaBIOS' vgabios implementation for QEmu from latest master
should work.

Build Options
-------------
Following options exist to change the default:
- CSM_ENABLE=TRUE: Expect Csm16.bin and build support for it
- SECURE_BOOT_ENABLE=TRUE: Build with SecureBoot support (broken
  because we lack nonvolatile variables)
- NO_PCI_ENUMERATION=FALSE: Build with PciBus drivers that re-enumerate
  and re-configure the entire PCI bus hierarchy.
- NETWORK_IP6_ENABLE=TRUE: Compile in IPv6 support

Native graphics init on QEmu
----------------------------
There's some issue that prevents the cirrus native graphics driver
in coreboot from initializing framebuffers over a certain size
(1024x768 or larger, ie. what Windows 8 requires).
To boot with native graphics on QEmu, use -vga std, which works.

Credits
-------
corebootPkg is a collaborative work. First, it's standing on the
shoulders of giants, namely the Tianocore project, from which it was
branched from. Hopefully we'll eventually get far enough to merge the
coreboot support there.

It was started as kind of a dare, which at times seems to be the main
driver of coreboot innovation.
"Thanks" Stefan Reinauer ;-)

He also contributed a bunch of interoperability patches, most importantly
memory table support.

David Woodhouse did the CSM port and provided some guidance when it came
to integration.

Scott Duplichan is (to my knowledge) the first one to seriously try
to get it running on real hardware.  Some of the emulator-specific
bits that corebootPkg inherited from OVMF are gone thanks to his code
slaying prowess.

Vladimir 'phcoder' Serbinenko added corebootPkg support to GRUB2's
coreboot loader, so it can chainload into UEFI.

-- Patrick Georgi
