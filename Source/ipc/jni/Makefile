OUT_DIR=../bin/java
OBJ_DIR=../bin/java/objects
CC=gcc
CFLAGS=-c -fPIC
LIB_PATH=../bin
PWD=$(shell pwd)
INCLUDE=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(JAVA_ARCH) -I../src
PACKAGE=com/mittudev/ipc

all: setup $(OUT_DIR)/libIPC.so $(OUT_DIR)/libIPC.dylib $(OUT_DIR)/IPC.jar

$(OUT_DIR)/libIPC.so: $(OBJ_DIR)/com_mittudev_ipc_Connection.o $(OBJ_DIR)/com_mittudev_ipc_Message.o
	$(CC) $(OBJ_DIR)/com_mittudev_ipc_Connection.o $(OBJ_DIR)/com_mittudev_ipc_Message.o -Xlinker -rpath -Xlinker $(PWD)/$(LIB_PATH)  -L$(LIB_PATH) -lipc -shared -o $@

$(OBJ_DIR)/com_mittudev_ipc_Connection.o:
	$(CC) $(CFLAGS) com_mittudev_ipc_Connection.c $(INCLUDE) -o $@

$(OBJ_DIR)/com_mittudev_ipc_Message.o:
	$(CC) $(CFLAGS) com_mittudev_ipc_Message.c $(INCLUDE) -o $@

$(OUT_DIR)/libIPC.dylib:
	cp $(OUT_DIR)/libIPC.so $(OUT_DIR)/libIPC.dylib

$(OUT_DIR)/IPC.jar: $(PACKAGE)/Connection.class $(PACKAGE)/Message.class $(PACKAGE)/ConnectionCallback.class
	jar -cf $@ $(PACKAGE)/*class

$(PACKAGE)/Connection.class:
	javac $(PACKAGE)/Connection.java

$(PACKAGE)/Message.class:
	javac $(PACKAGE)/Message.java

$(PACKAGE)/ConnectionCallback.class:
	javac $(PACKAGE)/ConnectionCallback.java

setup: $(OUT_DIR) $(OBJ_DIR)

$(OUT_DIR):
	mkdir $(OUT_DIR)

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

clean:
	rm $(PACKAGE)/*class
	rm $(OUT_DIR)/IPC.jar $(OUT_DIR)/libIPC.so $(OUT_DIR)/libIPC.dylib
	rm $(OBJ_DIR)/*o
