#
# SDK Makefile for x86 Linux
#
#

#############################################################################
# Developer configurable items
#############################################################################

# the name of the mod binary (_i486.so is appended to the end)
NAME=server

# the location of the vcproj that builds the mod
MOD_PROJ=../dlls/server_scratch-2003.vcproj
# the name of the mod configuration (typically <proj name>_<build type><build target>)
MOD_CONFIG=server_sdk_ReleaseSDKWin32

# the directory the base binaries (tier0_i486.so, etc) are located
#GAME_DIR=../../
#GAME_DIR=~/valve/hl2bin/
GAME_DIR=~/valve/steam

# compiler options (gcc 3.4.1 or above is required)
CC=gcc
CPLUS=g++
CLINK=gcc
CPP_LIB=

# put any compiler flags you want passed here
USER_CFLAGS=-ffast-math -fomit-frame-pointer
DEFINES=-DBUGFIXED -D_LINUX

# link flags for your mod, make sure to include any special libraries here
LDFLAGS="-lm -ldl tier1_i486.a mathlib_i486.a choreoobjects_i486.a tier0_i486.so vstdlib_i486.so"

# XERCES 2.6.0 or above ( http://xml.apache.org/xerces-c/ ) is used by the vcproj to makefile converter
# it must be installed before being able to run this makefile
XERCES_INC_DIR=/usr/include
XERCES_LIB_DIR=/usr/lib
# if you have xerces installed already you should be able to use the two lines below
#XERCES_INC_DIR=/usr/include
#XERCES_LIB_DIR=/usr/lib


#############################################################################
# Things below here shouldn't need to be altered
#############################################################################
MAKE=make
AR="i686-unknown-linux-gnu-ar rvs"

# the dir we want to put binaries we build into
BUILD_DIR=.
# the place to put object files
BUILD_OBJ_DIR=$(BUILD_DIR)/obj

# the location of the source code
SOURCE_DIR=..

# the CPU target for the build, must be i486 for now
ARCH=i486
ARCH_CFLAGS=-mtune=i686 -march=prescott -mfpmath=sse -mmmx -msse -msse2 -msse3 -O3

# -fpermissive is so gcc 3.4.x doesn't complain about some template stuff
BASE_CFLAGS=-fpermissive -D_LINUX -DNDEBUG -Dstricmp=strcasecmp -D_stricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp -D_vsnprintf=vsnprintf -D_alloca=alloca -Dstrcmpi=strcasecmp
SHLIBEXT=so
SHLIBCFLAGS=-fPIC
SHLIBLDFLAGS=-shared -Wl,-Map,$@_map.txt -Wl

#flags passed to the c compiler
CFLAGS="$(USER_CFLAGS) $(DEFINES) $(ARCH_CFLAGS) $(BASE_CFLAGS) -Usprintf -Ustrncpy -UPROTECTED_THINGS_ENABLE"

# define list passed to make for the sub makefile
BASE_DEFINES=CC=$(CC) CPLUS=$(CPLUS) CPP_LIB=$(CPP_LIB) MAKE=$(MAKE) AR=$(AR) \
	BUILD_DIR=$(BUILD_DIR) BUILD_OBJ_DIR=$(BUILD_OBJ_DIR) \
	SOURCE_DIR=$(SOURCE_DIR) SHLIBLDFLAGS=$(SHLIBLDFLAGS) SHLIBEXT=$(SHLIBEXT) \
	CLINK=$(CLINK) CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS) \
	ARCH=$(ARCH) GAME_DIR=$(GAME_DIR) MOD_CONFIG=$(MOD_CONFIG) NAME=$(NAME) \
	XERCES_INC_DIR=$(XERCES_INC_DIR) XERCES_LIB_DIR=$(XERCES_LIB_DIR)

# Project Makefile
MAKE_SERVER_CSTRIKE=Makefile.server_cstrike
MAKE_TIER1=Makefile.tier1
MAKE_MATH=Makefile.mathlib
MAKE_CHOREO=Makefile.choreoobjects

all: check server_cstrike

check:
	if [ -z "$(CC)" ]; then echo "Compiler not defined."; exit; fi
	if [ ! -d $(BUILD_DIR) ];then mkdir $(BUILD_DIR);fi
	cd $(BUILD_DIR)

server_cstrike:
	$(MAKE) -f $(MAKE_SERVER_CSTRIKE) $(BASE_DEFINES)

tier1:
	$(MAKE) -f $(MAKE_TIER1) $(BASE_DEFINES)

mathlib:
	$(MAKE) -f $(MAKE_MATH) $(BASE_DEFINES)

choreoobjects:
	$(MAKE) -f $(MAKE_CHOREO) $(BASE_DEFINES)
	
clean:
	$(MAKE) -f $(MAKE_SERVER_CSTRIKE) $(BASE_DEFINES) clean
	$(MAKE) -f $(MAKE_TIER1) $(BASE_DEFINES) clean
	$(MAKE) -f $(MAKE_MATH) $(BASE_DEFINES) clean
	$(MAKE) -f $(MAKE_CHOREO) $(BASE_DEFINES) clean
