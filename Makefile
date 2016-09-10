# Options:
# insert "USE_FDK-AAC=1" after the "make" call to use FDK-AAC instead of FAAD2 for audio decoding
# insert "DISABLE_SDL=1" after the "make" call to disable SDL output



CC = g++
CFLAGS = -Wall -std=c++0x
CFLAGS += -O0 -g3
#CFLAGS += -O3 -g0

INSTALL = install
prefix = /usr/local

# version derived from git (if possible)
VERSION_FROM_GIT = $(shell git describe --dirty 2> /dev/null)
ifneq "$(VERSION_FROM_GIT)" ""
	CFLAGS += -DDABLIN_VERSION=\"$(VERSION_FROM_GIT)\"
endif

CFLAGS_GTK = `pkg-config gtkmm-3.0 --cflags`

LDFLAGS = -lfec -lmpg123 -lpthread

LDFLAGS_GTK = `pkg-config gtkmm-3.0 --libs`

BIN_CLI = dablin
BIN_GTK = dablin_gtk

OBJ = eti_source.o eti_player.o dab_decoder.o dabplus_decoder.o fic_decoder.o pcm_output.o tools.o
OBJ_CLI =
OBJ_GTK = mot_manager.o pad_decoder.o
OBJ_BIN_CLI = $(BIN_CLI).o
OBJ_BIN_GTK = $(BIN_GTK).o

# if desired, use FDK-AAC instead of FAAD2
ifeq "$(USE_FDK-AAC)" "1"
	CFLAGS += -DDABLIN_AAC_FDKAAC
	LDFLAGS += -lfdk-aac
else
	CFLAGS += -DDABLIN_AAC_FAAD2
	LDFLAGS += -lfaad
endif

# if desired, disable SDL output
ifeq "$(DISABLE_SDL)" "1"
	CFLAGS += -DDABLIN_DISABLE_SDL
else
	CFLAGS += `sdl2-config --cflags`
	LDFLAGS += `sdl2-config --libs`
	OBJ += sdl_output.o
endif

# check for gtkmm (pkg-config returns 0 if present)
GTKMM_EXISTANCE_STATUS = $(shell pkg-config gtkmm-3.0 --exists 1>&2 2> /dev/null; echo $$?)
ifeq "$(GTKMM_EXISTANCE_STATUS)" "0"
	BIN = $(BIN_CLI) $(BIN_GTK)
	BIN_INSTALL = install_$(BIN_CLI) install_$(BIN_GTK)
else
	BIN = $(BIN_CLI)
	BIN_INSTALL = install_$(BIN_CLI)
endif



.PHONY: all
all: $(BIN)

$(BIN_CLI): $(OBJ) $(OBJ_CLI) $(OBJ_BIN_CLI)
	$(CC) -o $@ $(OBJ) $(OBJ_CLI) $(OBJ_BIN_CLI) $(LDFLAGS)

$(BIN_GTK): $(OBJ) $(OBJ_GTK) $(OBJ_BIN_GTK)
	$(CC) -o $@ $(OBJ) $(OBJ_GTK) $(OBJ_BIN_GTK) $(LDFLAGS_GTK) $(LDFLAGS)

$(OBJ_BIN_GTK): $(BIN_GTK).cpp
	$(CC) $(CFLAGS_GTK) $(CFLAGS) -c $<

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<


.PHONY: clean
clean:
	rm -rf $(BIN_CLI) $(BIN_GTK) $(OBJ) $(OBJ_CLI) $(OBJ_GTK) $(OBJ_BIN_CLI) $(OBJ_BIN_GTK)


.PHONY: install install_$(BIN_CLI) install_$(BIN_GTK)
install: all $(BIN_INSTALL)

install_$(BIN_CLI):
	$(INSTALL) $(BIN_CLI) -D $(DESTDIR)$(prefix)/bin/$(BIN_CLI)

install_$(BIN_GTK):
	$(INSTALL) $(BIN_GTK) -D $(DESTDIR)$(prefix)/bin/$(BIN_GTK)
