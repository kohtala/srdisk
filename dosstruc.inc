; DOS 5.0 structures as presented in Microsoft MS-DOS Programmers Reference

; Device Driver Header

DEVICEHEADER STRUC
  dhLink        dd ?
  dhAttributes  dw ?
  dhStrategy    dw ?
  dhInterrupt   dw ?
  dhNameOrUnits db '????????'
DEVICEHEADER ENDS

DHATTR_STDIN     equ 1
DHATTR_STDOUT    equ 2
DHATTR_32BIT     equ 2
DHATTR_NUL       equ 4
DHATTR_CLOCK     equ 8
DHATTR_INT29     equ 10h
DHATTR_GIOCTL    equ 40h
DHATTR_IOCTLQ    equ 80h
DHATTR_OPENCLOSE equ 800h
DHATTR_OUTBUSY   equ 2000h
DHATTR_NOFAT     equ 2000h
DHATTR_IOCTL     equ 4000h
DHATTR_CHARDEV   equ 8000h

; Device Driver Request Headers

REQUESTHEADER STRUC
  rhLength      db ?
  rhUnit        db ?
  rhFunction    db ?
  rhStatus      dw ?
  rhReserved    db 8 dup (?)
REQUESTHEADER ENDS

INITREQUEST STRUC
  irLength	 db ?
  irUnit	 db ?
  irFunction	 db ?
  irStatus	 dw ?
  irReserved	 db 8 dup (?)
  irUnits	 db ?
  irEndAddress	 dd ?
  irParamAddress dd ?
  irDriveNumber  db ?
  irMessageFlag  dw ?
INITREQUEST ENDS

MEDIAREQUEST STRUC
  mrLength	db ?
  mrUnit	db ?
  mrFunction	db ?
  mrStatus	dw ?
  mrReserved	db 8 dup (?)
  mrMediaID	db ?
  mrReturn	db ?
  mrVolumeID	dd ?
MEDIAREQUEST ENDS

BUILDBPBREQUEST STRUC
  bbrLength	db ?
  bbrUnit	db ?
  bbrFunction	db ?
  bbrStatus	dw ?
  bbrReserved	db 8 dup (?)
  bbrMediaID	db ?
  bbrFATSector	dd ?
  bbrBPBAddress dd ?
BUILDBPBREQUEST ENDS

IOCTLRWREQUEST STRUC
  irwrLength	db ?
  irwrUnit	db ?
  irwrFunction	db ?
  irwrStatus	dw ?
  irwrReserved	db 8 dup (?)
  irwrData	db ?
  irwrBuffer	dd ?
  irwrbytes	dw ?
IOCTLRWREQUEST ENDS

READWRITEREQUEST STRUC
  rwrLength	  db ?
  rwrUnit	  db ?
  rwrFunction	  db ?
  rwrStatus	  dw ?
  rwrReserved	  db 8 dup (?)
  rwrMediaID	  db ?
  rwrBuffer	  dd ?
  rwrBytesSec	  dw ?
  rwrStartSec	  dw ?
  rwrVolumeID	  dd ?
  rwrHugeStartSec dd ?
READWRITEREQUEST ENDS

IOCTLRWREQUEST STRUC
  irwrLength	   db ?
  irwrUnit	   db ?
  irwrFunction	   db ?
  irwrStatus	   dw ?
  irwrReserved	   db 8 dup (?)
  irwrData	   db ?
  irwrBuffer	   dd ?
  irwrBytes	   dw ?
IOCTLRWREQUEST ENDS

IOCTLREQUEST STRUC
  giLength	db ?
  giUnit	db ?
  giFunction	db ?
  giStatus	dw ?
  giReserved	db 8 dup (?)
  giCategory	db ?
  giMinorCode	db ?
  giReserved2	dd ?
  giIOCTLData	dd ?
IOCTLREQUEST ENDS

LOGDEVICEREQUEST STRUC
  ldrLength     db ?
  ldrUnit       db ?
  ldrFunction   db ?
  ldrStatus     dw ?
  ldrReserved   db 8 dup (?)
LOGDEVICEREQUEST ENDS

; rhStatus codes

DONE EQU 100h
BUSY EQU 200h
ERROR EQU 8000h

; errors
WRITE_PROTECT       EQU 0
UNKNOWN_UNIT        EQU 1
DRIVE_NOT_READY     EQU 2
UNKNOWN_COMMAND     EQU 3
CRC_ERROR           EQU 4
BAD_REQUEST_STRUC   EQU 5
SEEK_ERROR          EQU 6
UNKNOWN_MEDIA       EQU 7
SECTOR_NOT_FOUND    EQU 8
OUT_OF_PAPER        EQU 9
WRITE_FAULT         EQU 10
READ_FAULT          EQU 11
GENERAL_FAILURE     EQU 12
INVALID_DISK_CHANGE EQU 15

; DeviceParameters
; Used for Generic IOCTL functions get (60h) and set (40h) device params

DEVICEPARAMS STRUC
  dpSpecFunc    db ?
  dpDevType     db ?
  dpDevAttr     dw ?
  dpCylinders   dw ?
  dpMediaType   db ?
  dpBytesPerSec dw ?
  dpSecPerClust db ?
  dpResSectors  dw ?
  dpFATs        db ?
  dpRootDirEnts dw ?
  dpSectors     dw ?
  dpMedia       db ?
  dpFATsecs     dw ?
  dpSecPerTrack dw ?
  dpHeads       dw ?
  dpHiddenSecs  dd ?
  dpHugeSectors dd ?
DEVICEPARAMS ENDS

GETDPSF_DEFAULT EQU 0   ; Values for the dpSpecFunc for Get Device Parameters
GETDPSF_CURRENT EQU 1

; ReadWriteBlock
; Used for Generic IOCTL track read (61h) and write (41h) requests

RWBLOCK STRUC
  rwSpecFunc    db 0
  rwHead        dw ?
  rwCylinder    dw ?
  rwFirstSector dw ?
  rwSectors     dw ?
  rwBuffer      dd ?
RWBLOCK ENDS

; FormatVerifyBlock
; Used for Generic IOCTL track format (42h) and verify (62h) requests

; MediaID
; Used for Generic IOCTL

MID STRUC
  midInfoLevel   dw 0
  midSerialNum   dd ?
  midVolLabel    db 11 dup (?)
  midFileSysType db 8 dup (?)
MID ENDS

; Boot sector

BOOTSECTOR STRUC
  bsJump                db 3 dup(?)
  bsOemName             db '????????'

  bsBytesPerSec         dw ?
  bsSecPerClust         db ?
  bsResSectors          dw ?
  bsFATs                db ?
  bsRootDirEnts         dw ?
  bsSectors             dw ?
  bsMedia               db ?
  bsFATsecs             dw ?
  bsSecPerTrack         dw ?
  bsHeads               dw ?
  bsHiddenSecs          dd ?
  bsHugeSectors         dd ?

  bsDriveNumber         db ?
  bsReserved1           db ?
  bsBootSignature       db ?
  bsVolumeID            dd ?
  bsVolumeLabel         db 11 dup(?)
  bsFileSysType         db 8 dup(?)
BOOTSECTOR ENDS

