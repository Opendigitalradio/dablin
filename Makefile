# uncomment the following line to use FDK-AAC instead of FAAD2 for audio decoding:
#use_fdk-aac = true


CC = g++
CFLAGS = -Wall -std=c++0x `sdl2-config --cflags`
CFLAGS += -O0 -g3
#CFLAGS += -O3 -g0

CFLAGS_GTK = `pkg-config gtkmm-3.0 --cflags`

LDFLAGS = -lfec -lmpg123 -lpthread `sdl2-config --libs`

ifdef use_fdk-aac
	CFLAGS += -DDABLIN_AAC_FDKAAC
	LDFLAGS += -lfdk-aac
else
	CFLAGS += -DDABLIN_AAC_FAAD2
	LDFLAGS += -lfaad
endif

LDFLAGS_GTK = `pkg-config gtkmm-3.0 --libs`

BIN_CLI = dablin
BIN_GTK = dablin_gtk

OBJ = eti_source.o eti_player.o dab_decoder.o dabplus_decoder.o fic_decoder.o sdl_output.o tools.o
OBJ_CLI =
OBJ_GTK = pad_decoder.o
OBJ_BIN_CLI = $(BIN_CLI).o
OBJ_BIN_GTK = $(BIN_GTK).o


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
