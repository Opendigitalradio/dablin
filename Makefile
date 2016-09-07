# uncomment the following line to use FDK-AAC instead of FAAD2 for audio decoding:
#use_fdk-aac = true

# uncomment the following line to disable SDL output:
#disable_sdl = true


CC = g++
CFLAGS = -Wall -std=c++0x
CFLAGS += -O0 -g3
#CFLAGS += -O3 -g0

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

ifdef use_fdk-aac
	CFLAGS += -DDABLIN_AAC_FDKAAC
	LDFLAGS += -lfdk-aac
else
	CFLAGS += -DDABLIN_AAC_FAAD2
	LDFLAGS += -lfaad
endif

ifndef disable_sdl
	CFLAGS += `sdl2-config --cflags`
	LDFLAGS += `sdl2-config --libs`
	OBJ += sdl_output.o
else
	CFLAGS += -DDABLIN_DISABLE_SDL
endif


all: $(BIN_CLI) $(BIN_GTK)

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
