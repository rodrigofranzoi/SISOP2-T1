CC = gcc
INC_DIR = ./include
SRC_DIR = ./src
BIN_DIR = ./bin
DST_DIR = ./dst

all: utils client server

utils:	$(SRC_DIR)/utils.c 
	$(CC) -c $(SRC_DIR)/utils.c && mv utils.o $(BIN_DIR)

client:	$(SRC_DIR)/client.c
	$(CC) -o $(DST_DIR)/client $(SRC_DIR)/client.c $(BIN_DIR)/utils.o -pthread

server: $(SRC_DIR)/server.c
	$(CC) -o $(DST_DIR)/server $(SRC_DIR)/server.c $(BIN_DIR)/utils.o -pthread
