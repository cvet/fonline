JAVA_BIN=../../bin/java
CLASS_PATH=$(JAVA_BIN)/IPC.jar

all: Writer.class Reader.class

Writer.class:
	javac Writer.java -cp $(CLASS_PATH)

Reader.class:
	javac Reader.java -cp $(CLASS_PATH)

clean:
	rm *class
