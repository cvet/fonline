OUT_DIR=.
LIB_PATH=../../../bin/cpp
PWD=$(shell pwd)
INCLUDE_PATH=../../../cpp
CC=g++


all: $(OUT_DIR)/main

$(OUT_DIR)/main:
	$(CC) main.cpp -I$(INCLUDE_PATH) -L$(LIB_PATH) -Xlinker -rpath -Xlinker $(PWD)/$(LIB_PATH) -lIPC_CPP -o $(OUT_DIR)/$@


clean:
	rm $(OUT_DIR)/main
