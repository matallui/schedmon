BIN = bin
OBJ = obj
SRC = src
LIB =

CC = gcc
CFLAGS = -Wall -O2 -g
LIBS= -lrt -lpthread

TARGET = smon

objects := $(OBJ)/cmd.o $(OBJ)/driver.o $(OBJ)/rb.o $(OBJ)/profile.o $(OBJ)/main.o 

all: monitor

monitor: $(objects)
	$(CC) $(objects) $(LIBS) -o $(BIN)/$(TARGET)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) $(LIBS) -c $^ -o $@

clean:
	-rm -f $(OBJ)/*.o $(BIN)/* $(SRC)/*~

install:
	cp $(BIN)/$(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

