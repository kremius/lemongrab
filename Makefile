CC=g++
CFLAGS=-c -Wall -std=c++11 -I. -g
LDFLAGS=-lgloox -lpthread -lcurl -ljsoncpp

SRC=$(shell find . -name '*.cpp')
OBJ=$(SRC:.cpp=.o)
EXE=lemongrab

all: $(SRC) $(EXE)

clean:
	rm $(EXE)
	rm $(OBJ)

install:
	cp config.ini.default config.ini

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
