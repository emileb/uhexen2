# GNU Makefile for Hexen II Dedicated Server (h2ded) using GCC.
# $Id$
#
# It is ESSENTIAL that you run make clean between different
# types of builds or different types of targets.
#
# To cross-compile for Win32 on Unix, you must pass the W32BUILD=1
# argument to make. It would be best if you examine the script named
# build_cross_win32.sh for cross compilation.
#
# To cross-compile for Win64 on Unix, you must pass the W64BUILD=1
# argument to make. Also see build_cross_win64.sh for details.
#
# To (cross-)compile for DOS, you must pass the DOSBUILD=1 argument
# to make. You need djgpp compiler set. See the build_cross_dos.sh
# script for cross compilation on linux / unix.
#
# Build Options:
#
# OPT_EXTRA	yes  =  Some extra optimization flags will be added (default)
#		no   =	No extra optimizations will be made
#
# USE_WINSOCK2	yes  =	Use WinSock2 and link to ws2_32 instead of wsock32
# (for Win32)	no   =	(default) Use WinSock1.1 for compatibility with old
#			Windows 95 machines.
#
# COMPILE_32BITS yes =  Compile as a 32 bit binary. If you are on a 64 bit
#			platform and having problems with 64 bit compiled
#			binaries, set this option to yes. Default: no .
#			If you set this to yes, you need to have the 32 bit
#			versions of the libraries that you link against.
#		 no  =	Compile for the native word size of your platform,
#			which is the default option.
#
# The default compiler is gcc
# To build with a different compiler:	make CC=compiler_name [other stuff]
#
# To build for the demo version:	make DEMO=1 [other stuff]
#
# if building a debug version :		make DEBUG=1 [other stuff]
#

# Path settings:
# main uhexen2 relative path
UHEXEN2_TOP:=../..
LIBS_DIR:=$(UHEXEN2_TOP)/libs
# common sources path:
COMMONDIR:=../h2shared

# General options (see explanations at the top)
OPT_EXTRA=yes
COMPILE_32BITS=no

USE_WINSOCK2=no

# whether to use WatTCP for DOS UDP networking
USE_WATT32=yes
# whether to use Beame & Whiteside for DOS networking
USE_BWTCP=yes
# whether to use MPATH for DOS UDP networking under Win9x
USE_MPATH=yes

# include the common dirty stuff
include $(UHEXEN2_TOP)/scripts/makefile.inc

ifeq ($(TARGET_OS),win64)
# use winsock2 for win64
USE_WINSOCK2=yes
endif

# Names of the binaries
BINARY:=h2ded$(exe_ext)

#############################################################
# Compiler flags
#############################################################

ifeq ($(MACH_TYPE),x86)
CPU_X86=-march=i586
endif
# Overrides for the default CPUFLAGS
CPUFLAGS=$(CPU_X86)

# Overrides for the default ARCHFLAGS
#ARCHFLAGS=

CFLAGS += -g -Wall
CFLAGS += $(CPUFLAGS) $(ARCHFLAGS)

ifndef DEBUG
# optimization flags
CFLAGS += -O2 -DNDEBUG=1 -ffast-math -fexpensive-optimizations

ifeq ($(OPT_EXTRA),yes)
ifeq ($(MACH_TYPE),x86)
ALIGN_OPT:= $(call check_gcc,-falign-loops=2 -falign-jumps=2 -falign-functions=2,)
ifeq ($(ALIGN_OPT),)
ALIGN_OPT:= $(call check_gcc,-malign-loops=2 -malign-jumps=2 -malign-functions=2,)
endif
CFLAGS += $(ALIGN_OPT)
endif

ifeq ($(MACH_TYPE),x86_64)
ALIGN_OPT:= $(call check_gcc,-falign-loops=2 -falign-jumps=2 -falign-functions=2,)
ifeq ($(ALIGN_OPT),)
ALIGN_OPT:= $(call check_gcc,-malign-loops=2 -malign-jumps=2 -malign-functions=2,)
endif
CFLAGS += $(ALIGN_OPT)
endif

CFLAGS += -fomit-frame-pointer
endif
#
endif

CPPFLAGS=
LDFLAGS =

# compiler includes
INCLUDES= -I./server -I. -I$(COMMONDIR) -I$(LIBS_DIR)/common

ifeq ($(COMPILE_32BITS),yes)
CFLAGS += -m32
LDFLAGS+= -m32
endif

# end of compiler flags
#############################################################


#############################################################
# Other build flags
#############################################################
CPPFLAGS+= -DSERVERONLY

ifdef DEMO
CPPFLAGS+= -DDEMOBUILD
endif

ifdef DEBUG
# This activates some extra code in hexen2/hexenworld C source
CPPFLAGS+= -DDEBUG=1 -DDEBUG_BUILD=1
endif


#############################################################
# DOS flags/settings
#############################################################
ifeq ($(TARGET_OS),dos)

INCLUDES += -I$(OSLIBS)/dos
ifeq ($(USE_BWTCP),yes)
CPPFLAGS+= -DUSE_BWTCP
endif
ifeq ($(USE_MPATH),yes)
CPPFLAGS+= -DUSE_MPATH
endif
ifeq ($(USE_WATT32),yes)
CPPFLAGS+= -DUSE_WATT32
INCLUDES+= -I$(OSLIBS)/dos/watt32/inc
LDFLAGS += -L$(OSLIBS)/dos/watt32/lib -lwatt
endif
LDFLAGS += -lc -lgcc -lm

endif
# End of DOS settings
#############################################################


#############################################################
# Win32 flags/settings
#############################################################
ifeq ($(TARGET_OS),win32)

CFLAGS += -DWIN32_LEAN_AND_MEAN

ifeq ($(USE_WINSOCK2),yes)
CPPFLAGS+= -D_USE_WINSOCK2
LIBWINSOCK=-lws2_32
else
LIBWINSOCK=-lwsock32
endif

INCLUDES+= -I$(OSLIBS)/windows
LDFLAGS += $(LIBWINSOCK) -lwinmm -mconsole

endif
# End of Win32 settings
#############################################################


#############################################################
# Win64 flags/settings
#############################################################
ifeq ($(TARGET_OS),win64)

CFLAGS += -DWIN32_LEAN_AND_MEAN

ifeq ($(USE_WINSOCK2),yes)
CPPFLAGS+= -D_USE_WINSOCK2
LIBWINSOCK=-lws2_32
else
LIBWINSOCK=-lwsock32
endif

INCLUDES+= -I$(OSLIBS)/windows
LDFLAGS += $(LIBWINSOCK) -lwinmm -mconsole

endif
# End of Win64 settings
#############################################################


#############################################################
# Unix flags/settings
#############################################################
ifeq ($(TARGET_OS),unix)
LDFLAGS += $(LIBSOCKET) -lm

endif
# End of Unix settings
#############################################################


# Rules for turning source files into .o files
%.o: server/%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -o $@ $<
%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -o $@ $<
%.o: $(COMMONDIR)/%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -o $@ $<
%.o: $(LIBS_DIR)/common/%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -o $@ $<

# Objects

# Platform specific object settings
ifeq ($(TARGET_OS),win32)
SYSOBJ_NET := net_win.o net_wins.o net_wipx.o
SYSOBJ_SYS := sys_win.o
endif
ifeq ($(TARGET_OS),win64)
SYSOBJ_NET := net_win.o net_wins.o net_wipx.o
SYSOBJ_SYS := sys_win.o
endif
ifeq ($(TARGET_OS),dos)
DOSTCP :=
ifeq ($(USE_BWTCP),yes)
DOSTCP += dos/net_bw.o
endif
ifeq ($(USE_MPATH),yes)
DOSTCP += dos/net_mp.o dos/mplpc.o
endif
ifeq ($(USE_WATT32),yes)
DOSTCP += net_udp.o
else
# get inet_addr() and inet_ntoa() either from Watt-32
# or from our local implementation
DOSTCP += dos/dos_inet.o dos/inet_addr.o
endif
SYSOBJ_NET := dos/net_dos.o dos/net_ser.o $(DOSTCP) dos/net_ipx.o
SYSOBJ_SYS := dos_v2.o sys_dos.o
endif
ifeq ($(TARGET_OS),unix)
SYSOBJ_NET := net_bsd.o net_udp.o
SYSOBJ_SYS := sys_unix.o
endif

# Final list of objects
SV_OBJS = \
	q_endian.o \
	link_ops.o \
	sizebuf.o \
	strlcat.o \
	strlcpy.o \
	qsnprint.o \
	msg_io.o \
	common.o \
	debuglog.o \
	quakefs.o \
	cmd.o \
	crc.o \
	cvar.o \
	mathlib.o \
	zone.o \
	$(SYSOBJ_NET) \
	net_dgrm.o \
	net_main.o \
	model.o \
	host.o \
	host_cmd.o \
	pr_cmds.o \
	pr_edict.o \
	pr_exec.o \
	host_string.o \
	sv_effect.o \
	sv_main.o \
	sv_move.o \
	sv_phys.o \
	sv_user.o \
	world.o \
	$(SYSOBJ_SYS)


# Targets
.PHONY: clean distclean report

default: $(BINARY)
all: default

$(BINARY): $(SV_OBJS)
	$(LINKER) -o $(BINARY) $(SV_OBJS) $(LDFLAGS)

# deps for *.c including another *.c
dos/mplpc.o: dos/mplib.c
dos/net_ser.o: dos/net_comx.c

clean:
	rm -f *.o *.res core
distclean: clean
	rm -f $(BINARY)

report:
	@echo "Host OS  :" $(HOST_OS)
	@echo "Target OS:" $(TARGET_OS)
	@echo "Machine  :" $(MACH_TYPE)

