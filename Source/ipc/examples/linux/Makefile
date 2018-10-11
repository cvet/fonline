OUT_DIR=.
LIB_PATH=../../bin
PWD=$(shell pwd)
INCLUDE_PATH=../../src
CC=gcc


all: $(OUT_DIR)/main

$(OUT_DIR)/main:
	$(CC) -pthread main.c -I$(INCLUDE_PATH) -L$(LIB_PATH) -Xlinker -rpath -Xlinker $(PWD)/$(LIB_PATH) -lipc -o $(OUT_DIR)/$@


clean:
	rm $(OUT_DIR)/main
