@ Luma3DS custom system call svcControlService (0xB0), used to get a session to
@ PxiFS0 for GBA Virtual Console saves (the same way PKSM does it). Needs Luma3DS.
@ Based on Luma3DS's csvc stub (zlib licence; see Luma3DS's sysmodules/csvc).

    .section .text.svcControlService, "ax", %progbits
    .global svcControlService
    .type svcControlService, %function
    .align 2
svcControlService:
    svc 0xB0
    bx lr
