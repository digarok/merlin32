# GNUmakefile 
#   - Supports building with GNU make & gcc
#   - Supports building on Linux, Mac OS, BSD, and Windows
#     assuming that you have GNU make an gcc on those systems.
#   - Automatically separates GNU make builds from other make systems
#     like BSD make and Microsoft's nmake.exe, because
#     GNU make looks for GNUmakefile first and other makes don't
#     look for it at all.
#   - Compiler can be customized on the command line like this: 
#       make CC=gcc-apple-4.2
#   - Install directory can be customized on the command line like this:
#       make PREFIX=/opt/merlin32
#     The default PREFIX is /usr/local.


CFLAGS_DEBUG=-g
CFLAGS_OPT=-O3

TARGET=merlin32$(EXE_EXT)
CFLAGS+=-Wall -DMACRO_DIR=\"$(MACRO_DIR)\"
BIN_CONFIG=../Scripts/config.sh

ifeq ($(OS),Windows_NT)
    ifndef PREFIX
    PREFIX=$(USERPROFILE)/Applications/Merlin32
    endif
    MERLIN_BIN=$(PREFIX)/bin
    MACRO_DIR=$(PREFIX)/asminc
    EXE_EXT=.exe
else
    ifndef PREFIX
    PREFIX=/usr/local
    endif
    MERLIN_BIN=$(PREFIX)/bin
    MACRO_DIR=$(PREFIX)/share/merlin32/asminc
    EXE_EXT=
    
    OS:=$(shell uname -s)
    CPU:=$(shell uname -p)
    ifeq ($(OS),Darwin)
        # add both mac architectures to create a fat universal binary
        CFLAGS+=-arch x86_64 
        CFLAGS+=-arch arm64
    endif
    
    ifdef CPU_OPT
        ifeq ($(OS),Darwin)
            MAH_SHEEN:=$(shell machine)
            ifeq ($(CPU),powerpc)
                CFLAGS+=-fast
                ifeq ($(MAH_SHEEN),ppc7450)
                    CFLAGS+=-mcpu=7450
                endif
            endif
        endif
    endif

    ifeq ($(MAKECMDGOALS),debug)
        override CFLAGS+=$(CFLAGS_DEBUG)
    else
        override CFLAGS+=$(CFLAGS_OPT)
    endif

endif



SOURCES = a65816_Code.c a65816_Cond.c a65816_Data.c a65816_File.c \
	  a65816_Line.c a65816_Link.c a65816_Lup.c a65816_Macro.c a65816_OMF.c \
	  Dc_Library.c Main.c 

OBJECTS=$(SOURCES:.c=.o)

all: $(TARGET)

debug: $(TARGET)

$(TARGET): $(OBJECTS) $(BIN_CONFIG)
	$(CC) $(OBJECTS) -o $@

$(BIN_CONFIG):
	echo PREFIX=$(PREFIX) > $(BIN_CONFIG)
	echo INSTALL_BIN=$(MERLIN_BIN) >> $(BIN_CONFIG)
	echo MACRO_INSTALL_DIR=$(MACRO_DIR) >> $(BIN_CONFIG)

install: $(TARGET)
	../Scripts/install

installer: $(TARGET)
	../Scripts/make-installer

test: $(TARGET)
	$(MERLIN_BIN)/$(TARGET) ../Test/linkscript.s

clean:
	$(RM) $(OBJECTS) $(TARGET) $(BIN_CONFIG)
	rm -rf ../Installer

