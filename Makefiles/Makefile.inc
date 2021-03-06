##
## Supermodel
## A Sega Model 3 Arcade Emulator.
## Copyright 2011-2017 Bart Trzynadlowski, Nik Henson, Ian Curtis
##
## This file is part of Supermodel.
##
## Supermodel is free software: you can redistribute it and/or modify it under
## the terms of the GNU General Public License as published by the Free 
## Software Foundation, either version 3 of the License, or (at your option)
## any later version.
##
## Supermodel is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
## FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
## more details.
##
## You should have received a copy of the GNU General Public License along
## with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
##

#
# Makefile.inc
#
# Core GNU Makefile, included by all (platform-specific) top-level Makefiles.
#


###############################################################################
# Build Options
###############################################################################

#
# Verbose progression
#
VERBOSE =
ifneq ($(filter $(strip $(VERBOSE)),0 1),$(strip $(VERBOSE)))
	override VERBOSE =
endif
SILENT = @
ifeq ($(strip $(VERBOSE)),1)
	SILENT =
endif

#
# Enable render state analyser for the legacy 3D engine (will slow down emulation!)
#
DEBUG =
ifneq ($(filter $(strip $(DEBUG)),0 1),$(strip $(DEBUG)))
	override DEBUG =
endif

#
# Enable support for Model3 Net Board emulation
#
NET_BOARD =
ifneq ($(filter $(strip $(NET_BOARD)),0 1),$(strip $(NET_BOARD)))
	override NET_BOARD =
endif

#
# Enable Bart's new (experimental) Real3D frame timing
#
NEW_FRAME_TIMING =
ifneq ($(filter $(strip $(NEW_FRAME_TIMING)),0 1),$(strip $(NEW_FRAME_TIMING)))
	override NEW_FRAME_TIMING =
endif

#
# Include console-based debugger in emulator ('yes' or 'no')
#
ENABLE_DEBUGGER =
ifneq ($(filter $(strip $(ENABLE_DEBUGGER)),0 1),$(strip $(ENABLE_DEBUGGER)))
	override ENABLE_DEBUGGER =
endif


###############################################################################
# Source Files
###############################################################################

SRC_FILES = \
	Src/CPU/PowerPC/PPCDisasm.cpp \
	Src/BlockFile.cpp \
	Src/Pkgs/unzip.cpp \
	Src/Pkgs/ioapi.cpp \
	Src/Model3/93C46.cpp \
	Src/Util/BitRegister.cpp \
	Src/JTAG.cpp \
	Src/Graphics/Legacy3D/Error.cpp \
	Src/Pkgs/glew.cpp \
	Src/Graphics/Shader.cpp \
	Src/Model3/Real3D.cpp \
	Src/Graphics/Legacy3D/Legacy3D.cpp \
	Src/Graphics/Legacy3D/Models.cpp \
	Src/Graphics/Legacy3D/TextureRefs.cpp \
	Src/Graphics/New3D/New3D.cpp \
	Src/Graphics/New3D/Mat4.cpp \
	Src/Graphics/New3D/Model.cpp \
	Src/Graphics/New3D/PolyHeader.cpp \
	Src/Graphics/New3D/Texture.cpp \
	Src/Graphics/New3D/TextureSheet.cpp \
	Src/Graphics/New3D/VBO.cpp \
	Src/Graphics/New3D/Vec.cpp \
	Src/Graphics/New3D/R3DShader.cpp \
	Src/Graphics/New3D/R3DFloat.cpp \
	Src/Graphics/New3D/R3DScrollFog.cpp \
	Src/Graphics/Render2D.cpp \
	Src/Model3/TileGen.cpp \
	Src/Model3/Model3.cpp \
	Src/CPU/PowerPC/ppc.cpp \
	Src/Model3/SoundBoard.cpp \
	Src/Sound/SCSP.cpp \
	Src/Sound/SCSPDSP.cpp \
	Src/CPU/68K/68K.cpp \
	$(OBJ_DIR)/m68kcpu.c \
	$(OBJ_DIR)/m68kopnz.c \
	$(OBJ_DIR)/m68kopdm.c \
	$(OBJ_DIR)/m68kopac.c \
	$(OBJ_DIR)/m68kops.c \
	Src/Model3/DSB.cpp \
	Src/CPU/Z80/Z80.cpp \
	Src/Model3/IRQ.cpp \
	Src/Model3/53C810.cpp \
	Src/Model3/PCI.cpp \
	Src/Model3/RTC72421.cpp \
	Src/Model3/DriveBoard.cpp \
	Src/Model3/MPC10x.cpp \
	Src/Inputs/Input.cpp \
	Src/Inputs/Inputs.cpp \
	Src/Inputs/InputSource.cpp \
	Src/Inputs/InputSystem.cpp \
	Src/Inputs/InputTypes.cpp \
	Src/Inputs/MultiInputSource.cpp \
	Src/OSD/SDL/SDLInputSystem.cpp \
	Src/OSD/Outputs.cpp \
	Src/Sound/MPEG/amp_audio.cpp \
	Src/Sound/MPEG/dump.cpp \
	Src/Sound/MPEG/getbits.cpp \
	Src/Sound/MPEG/getdata.cpp \
	Src/Sound/MPEG/huffman.cpp \
	Src/Sound/MPEG/layer2.cpp \
	Src/Sound/MPEG/layer3.cpp \
	Src/Sound/MPEG/misc2.cpp \
	Src/Sound/MPEG/position.cpp \
	Src/Sound/MPEG/transform.cpp \
	Src/Sound/MPEG/util.cpp \
	Src/Model3/Crypto.cpp \
	Src/OSD/Logger.cpp \
	Src/Util/Format.cpp \
	Src/Util/NewConfig.cpp \
	Src/Util/ByteSwap.cpp \
	Src/Util/ConfigBuilders.cpp \
	Src/GameLoader.cpp \
	Src/Pkgs/tinyxml2.cpp \
	Src/ROMSet.cpp \
	$(PLATFORM_SRC_FILES)

ifeq ($(strip $(NET_BOARD)),1)
	SRC_FILES += \
		Src/Network/UDPReceive.cpp \
		Src/Network/UDPSend.cpp \
		Src/Network/WinSockWrap.cpp \
		Src/Network/NetBoard.cpp
endif

ifeq ($(strip $(ENABLE_DEBUGGER)),1)
	SRC_FILES += \
		Src/Debugger/Debugger.cpp \
		Src/Debugger/ConsoleDebugger.cpp \
		Src/Debugger/SupermodelDebugger.cpp \
		Src/Debugger/CPUDebug.cpp \
		Src/Debugger/AddressTable.cpp \
		Src/Debugger/Breakpoint.cpp \
		Src/Debugger/CodeAnalyser.cpp \
		Src/Debugger/Exception.cpp \
		Src/Debugger/Interrupt.cpp \
		Src/Debugger/DebuggerIO.cpp \
		Src/Debugger/Label.cpp \
		Src/Debugger/Register.cpp \
		Src/Debugger/Watch.cpp \
		Src/Debugger/CPU/PPCDebug.cpp \
		Src/Debugger/CPU/68KDebug.cpp \
		Src/Debugger/CPU/Musashi68KDebug.cpp \
		Src/Debugger/CPU/Z80Debug.cpp
endif

#
# Sorted-path compile order
#
OBJ_FILES = $(foreach file,$(SRC_FILES),$(OBJ_DIR)/$(basename $(notdir $(file))).o)

#
# Deduce include directories from the source file list. The sort function 
# removes duplicates and is used to construct a set.
#
INCLUDE_DIRS = $(sort $(foreach file,$(SRC_FILES),$(dir	$(file))))


###############################################################################
# Output Locations
###############################################################################

OBJ_DIR = obj$(strip $(BITS))
BIN_DIR = bin$(strip $(BITS))

OUTFILE = supermodel


###############################################################################
# Compiler Flags
###############################################################################

ARCH = -march=native -mfpmath=sse
BUILD_CFLAGS =
OPT = -O3
WARN = -Wall
CSTD = -std=iso9899:2011
CXXSTD = -std=c++11

COMMON_CFLAGS = -c $(ARCH) $(OPT) $(WARN) $(addprefix -I,$(sort $(INCLUDE_DIRS))) -DGLEW_STATIC

CFLAGS = $(COMMON_CFLAGS) $(CSTD)
CXXFLAGS = $(PLATFORM_CFLAGS) $(COMMON_CFLAGS) $(BUILD_CFLAGS) $(CXXSTD)
LDFLAGS = -o $(BIN_DIR)/$(OUTFILE) $(PLATFORM_LDFLAGS) -s

# If render state analyser is enabled, need to define DEBUG
ifeq ($(strip $(DEBUG)),1)
	BUILD_CFLAGS += -DDEBUG
endif

# If Net Board support is enabled, need to define NET_BOARD
ifeq ($(strip $(NET_BOARD)),1)
	BUILD_CFLAGS += -DNET_BOARD
endif

# If Bart's new frame timing is enabled, need to define NEW_FRAME_TIMING
ifeq ($(strip $(NEW_FRAME_TIMING)),1)
	BUILD_CFLAGS += -DNEW_FRAME_TIMING
endif

# If built-in debugger enabled, need to define SUPERMODEL_DEBUGGER
ifeq ($(strip $(ENABLE_DEBUGGER)),1)
	BUILD_CFLAGS += -DSUPERMODEL_DEBUGGER
endif


###############################################################################
# Targets
###############################################################################

# 
# Default target: build Supermodel binary. Must be specified *before* the auto-
# generated dependencies because otherwise, make gets confused for some reason
# and thinks the default target is just one of the object files.
#
all:	$(BIN_DIR)/$(OUTFILE)

$(BIN_DIR)/$(OUTFILE):	$(BIN_DIR) $(OBJ_DIR) $(OBJ_FILES)
	$(info --------------------------------------------------------------------------------)
	$(info Linking Supermodel     : $(BIN_DIR)/$(OUTFILE))
	$(SILENT)$(LD) $(OBJ_FILES) $(LDFLAGS)
	$(info --------------------------------------------------------------------------------)

$(BIN_DIR):
	$(info Creating directory     : $(BIN_DIR))
	$(SILENT)mkdir $(BIN_DIR) 

$(OBJ_DIR):
	$(info Creating directory     : $(OBJ_DIR))
	$(SILENT)mkdir $(OBJ_DIR)


###############################################################################
# Rules
###############################################################################

#
# Create list of auto-generated dependency files (which contain rules that make
# understands) and include them all.
#
AUTODEPS := $(patsubst %.o,%.d,$(OBJ_FILES))
-include $(AUTODEPS)

#
# VPATH is the search path for files. This trick allows a single %.cpp rule to
# be matched against files in sub-directories of Src/.
#
VPATH = $(INCLUDE_DIRS)

#
# Compilation rules that both auto-generate the dependency files and compile
# the source code. This technique is described in the reply by user "rr-" at:
# https://stackoverflow.com/questions/8025766/makefile-auto-dependency-generation
#
$(OBJ_DIR)/%.o:	%.cpp
	$(info Generating dependencies: $< -> $(OBJ_DIR)/$(*F).d)
	$(SILENT)$(CXX) -MM -MP -MT $(OBJ_DIR)/$(*F).o -MT $(OBJ_DIR)/$(*F).d $(CXXFLAGS) $< > $(OBJ_DIR)/$(*F).d
	$(info Compiling              : $< -> $@)
	$(SILENT)$(CXX) $(CXXFLAGS) $< -o $@

$(OBJ_DIR)/%.o:	%.c
	$(info Generating dependencies: $< -> $(OBJ_DIR)/$(*F).d)
	$(SILENT)$(CC) -MM -MP -MT $(OBJ_DIR)/$(*F).o -MT $(OBJ_DIR)/$(*F).d $(CFLAGS) $< > $(OBJ_DIR)/$(*F).d
	$(info Compiling              : $< -> $@)
	$(SILENT)$(CC) $(CFLAGS) $< -o $@


#
# Musashi 68K emulator
#
# All generated source files are emitted to the object directory. For MSVC,
# INLINE must be redefined as "static __inline", which is the syntax in C mode.
#
# The .exe suffix is absolutely required for native Windows non-MSYS builds! 
#

MUSASHI_OUTFILE = $(OBJ_DIR)/m68kmake.exe # do not remove the .exe suffix!
MUSASHI_CFLAGS = -ISrc/CPU/68K/Musashi -I$(OBJ_DIR) -DINLINE="static inline" -Wno-unused-variable
MUSASHI_LDFLAGS = -o $(MUSASHI_OUTFILE) $(OBJ_DIR)/m68kmake.o -s

$(MUSASHI_OUTFILE): Src/CPU/68K/Musashi/m68kmake.c Src/CPU/68K/Musashi/m68k_in.c
	$(info --------------------------------------------------------------------------------)
	$(info Compiling              : $< -> $(OBJ_DIR)/m68kmake.o)
	$(SILENT)$(CC) $< $(CFLAGS) -o $(OBJ_DIR)/m68kmake.o
	$(info Linking                : $(MUSASHI_OUTFILE))
	$(SILENT)$(LD) $(MUSASHI_LDFLAGS)

$(OBJ_DIR)/m68kops.h $(OBJ_DIR)/m68kops.c $(OBJ_DIR)/m68kopac.c $(OBJ_DIR)/m68kopdm.c $(OBJ_DIR)/m68kopnz.c:	$(MUSASHI_OUTFILE) Src/CPU/68K/Musashi/m68k_in.c Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h
	$(info Generating 68K emulator: $@)
	@$(MUSASHI_OUTFILE) $(OBJ_DIR) Src/CPU/68K/Musashi/m68k_in.c
	
$(OBJ_DIR)/m68kcpu.o:	Src/CPU/68K/Musashi/m68kcpu.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $(OBJ_DIR)/m68kcpu.o

$(OBJ_DIR)/m68kops.o:	$(OBJ_DIR)/m68kops.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE) 
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopac.o: $(OBJ_DIR)/m68kopac.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE) 
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopdm.o: $(OBJ_DIR)/m68kopdm.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE) 
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@

$(OBJ_DIR)/m68kopnz.o: $(OBJ_DIR)/m68kopnz.c $(OBJ_DIR)/m68kops.h Src/CPU/68K/Musashi/m68k.h Src/CPU/68K/Musashi/m68kconf.h $(MUSASHI_OUTFILE) 
	$(info Compiling              : $< -> $@)
	@$(CC) $< $(CFLAGS) $(MUSASHI_CFLAGS) -o $@
