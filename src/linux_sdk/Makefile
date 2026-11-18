#
# SDK Makefile for x86 Linux
#
# Targets:
#   make              build server_i486.so from the game sources
#   make -j8          the same, in parallel (recommended)
#   make libs         rebuild the tier1/mathlib/choreoobjects archives
#   make clean        delete all objects, the map file and the binaries
#

#############################################################################
# Developer configurable items
#############################################################################

# the name of the mod binary (_i486.so is appended to the end)
NAME=server

# compiler options (gcc 3.4.x or above is required)
CC=i686-unknown-linux-gnu-gcc
CPLUS=i686-unknown-linux-gnu-g++
CLINK=i686-unknown-linux-gnu-gcc
CPP_LIB="/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/i686-unknown-linux-gnu/lib/libstdc++.a /opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/lib/gcc/i686-unknown-linux-gnu/3.4.6/libgcc_eh.a"

# put any compiler flags you want passed here
USER_CFLAGS=-ffast-math -fomit-frame-pointer
DEFINES=

# link flags for your mod, make sure to include any special libraries here
LDFLAGS="-lm -ldl tier1_i486.a mathlib_i486.a choreoobjects_i486.a tier0_i486.so vstdlib_i486.so"

#############################################################################
# Things below here shouldn't need to be altered
#############################################################################

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
SHLIBLDFLAGS=-shared

# -MMD -MP writes a .d file next to every object listing the headers it used,
# so a rebuild only recompiles the files whose headers actually changed.
# -pipe avoids temporary files between the preprocessor and the compiler.
DEPFLAGS=-MMD -MP

#flags passed to the c compiler
CFLAGS="$(USER_CFLAGS) $(DEFINES) $(ARCH_CFLAGS) $(BASE_CFLAGS) -pipe -Usprintf -Ustrncpy -UPROTECTED_THINGS_ENABLE"

# define list passed to make for the sub makefile
BASE_DEFINES=CC=$(CC) CPLUS=$(CPLUS) CPP_LIB=$(CPP_LIB) AR=$(AR) \
	BUILD_DIR=$(BUILD_DIR) BUILD_OBJ_DIR=$(BUILD_OBJ_DIR) \
	SOURCE_DIR=$(SOURCE_DIR) SHLIBLDFLAGS=$(SHLIBLDFLAGS) SHLIBEXT=$(SHLIBEXT) \
	CLINK=$(CLINK) CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS) \
	DEPFLAGS="$(DEPFLAGS)" ARCH=$(ARCH) NAME=$(NAME)

# Project Makefile
MAKE_SERVER_CSTRIKE=Makefile.server_cstrike
MAKE_TIER1=Makefile.tier1
MAKE_MATH=Makefile.mathlib
MAKE_CHOREO=Makefile.choreoobjects

.PHONY: all server_cstrike libs tier1 mathlib choreoobjects clean

all: server_cstrike

# The DLL is linked against the prebuilt tier1/mathlib/choreoobjects archives
# that ship with the tree, so a plain build only compiles the game sources.
server_cstrike:
	$(MAKE) -f $(MAKE_SERVER_CSTRIKE) $(BASE_DEFINES)

# Rebuild those archives from source. Needed only after editing tier1, mathlib
# or game/shared (choreo) sources, or when switching toolchains.
libs: tier1 mathlib choreoobjects

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
