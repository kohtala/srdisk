# This is Borland MAKE makefile

# Files to fix for new version:
# file_id.diz - version and copyright
# *.doc - version, copyright, changes
# srdisk.h - VERSION
# srdisk.c - copyright year
# define.inc - SRD_VERSION, DEBUGGING
# srdisk.inc - copyright year
# srdummy.asm - copyright year
# makefile - VERSION and RELTIME

BORLANDC=d:\borlandc

!ifndef DISTRIBUTE
DEBUG=/v
TEST=-N
LINK=-m -s
BETA=-DBETA
!else
TEST=-DNDEBUG
!endif
MODEL=s
CFLAGS=-O2 -w -m$(MODEL) $(DEBUG) $(TEST)

.AUTODEPEND

OBJS =\
 allocate.obj \
 bitmap.obj \
 cmdline.obj \
 director.obj \
 diskinit.obj \
 diskio.obj \
 env.obj \
 error.obj \
 fat.obj \
 format.obj \
 formutil.obj \
 getopt.obj \
 makenewf.obj \
 packdata.obj \
 realloc.obj \
 resize.obj \
 srdemsf.obj \
 srdisk.obj \
 srdutil.obj \
 srdxmsf.obj \
 writenew.obj

#Make the version always three digits
VERSION = 209

# The time format is mmddhhmmyy.ss
RELTIME = 07150$(VERSION)05.00

ARCHIVE = srdsk$(VERSION)

all: srdisk.exe srdxms.sys srdems.sys srdxmss.sys srdemss.sys srdems3.sys \
 srdummy.sys xmssize.exe

.SUFFIXES: .exe .sys .obj .asm .c 

.c.obj:
        bcc $(CFLAGS) $(BETA) -c{ $&.c}

.obj.sys:
        tlink /t $&.obj,$@

.asm.obj:
        tasm /t /m2 /ml /la $&.asm

srdxms.obj srdxmss.obj: srdxms.inc srdisk.inc define.inc
srdems.obj srdemss.obj: srdems.inc srdisk.inc define.inc
srdems3.obj: srdems3.inc srdisk.inc define.inc
srdummy.obj: define.inc

srdisk.exe: $(OBJS)
        tlink /L$(BORLANDC)\lib /c $(DEBUG) $(LINK) c0$(MODEL).obj @&&!
$(OBJS)
!,srdisk.exe,,c$(MODEL).lib

xmssize.exe: xmssize.obj
        tlink xmssize

distribute:
        del *.obj
        del srdisk.exe
        del *.sys
        $(MAKE) "-DDISTRIBUTE" distribution

distribution: all package
        echo :::::: DEBUG=$(DEBUG) TEST=$(TEST) BETA=$(BETA)

package:
        touch -t$(RELTIME) file_id.diz whatsnew.doc beta.doc read.me
        touch -t$(RELTIME) *.sys srdisk.doc srdisk.exe suomi.doc 
	touch -t$(RELTIME) xmssize.doc xmssize.exe
        pkzip -ex $(ARCHIVE) @&&!
read.me
file_id.diz
whatsnew.doc
srdisk.doc
srdxms.sys
srdxmss.sys
srdems.sys
srdemss.sys
srdems3.sys
srdummy.sys
srdisk.exe
xmssize.doc
xmssize.exe
COPYING
!

