package com.mittudev.ipc;

public class Message{
	static{
		System.loadLibrary("IPC");
	}
	protected long ptr;

	protected Message(long ptr){
		this.ptr = ptr;
	}

	public Message(byte[] data){
		ptr = messageCreate(data);
	}

	public void setPID(int pid){
		messageSetPID(ptr, pid);
	}

	public void setSubject(String subject){
		messageSetSubject(ptr, subject);
	}

	public byte[] getData(){
		return messageGetData(ptr);
	}

	public String getSubject(){
		return messageGetSubject(ptr);
	}

	public long getPID(){
		return messageGetPID(ptr);
	}

	public int getType(){
		return messageGetType(ptr);
	}

	public void destroy(){
		messageDestroy(ptr);
	}

	public native long messageCreate(byte[] data);
	public native void messageSetPID(long ptr, int pid);
	public native void messageSetSubject(long ptr, String subject);
	public native byte[] messageGetData(long ptr);
	public native String messageGetSubject(long ptr);
	public native long messageGetPID(long ptr);
	public native int messageGetType(long ptr);
	public native void messageDestroy(long ptr);
}
