package com.mittudev.ipc;

import com.mittudev.ipc.Message;
import com.mittudev.ipc.ConnectionCallback;

public class Connection{
	static{
		System.loadLibrary("IPC");
	}
	private long ptr;
	private ConnectionCallback cb;

	private Connection(long ptr){
		this.ptr = ptr;
	}

	public Connection(String name, int type){
		ptr = connectionCreate(name, type);
	}

	public static Connection connect(String name, int type){
		return new Connection(connectionConnect(name, type));
	}

	public void startAutoDispatch(){
		connectionStartAutoDispatch(ptr);
	}

	public void stopAutoDispatch(){
		connectionStopAutoDispatch(ptr);
	}

	public void setCallback(ConnectionCallback cb){
		this.cb = cb;
		connectionSetCallback(ptr, cb);
	}

	public ConnectionCallback getCallback(){
		return cb;
	}

	public void removeCallback(){
		this.cb = null;
		connectionRemoveCallback(ptr);
	}

	public void send(Message msg){
		connectionSend(ptr, msg.ptr);
	}

	public void subscribe(String subject){
		connectionSubscribe(ptr, subject);
	}

	public void removeSubscription(String subject){
		connectionRemoveSubscription(ptr, subject);
	}

	public void close(){
		connectionClose(ptr);
	}

	public void destroy(){
		connectionDestroy(ptr);
	}

	private native static long connectionCreate(String name, int type);
	private native static long connectionConnect(String name, int type);
	private native void connectionStartAutoDispatch(long ptr);
	private native void connectionStopAutoDispatch(long ptr);
	private native void connectionSetCallback(long ptr, ConnectionCallback cb);
	private native void connectionRemoveCallback(long ptr);
	private native void connectionSend(long ptr, long msgPtr);
	private native void connectionSubscribe(long ptr, String subject);
	private native void connectionRemoveSubscription(long ptr, String subject);
	private native void connectionDestroy(long ptr);
	private native void connectionClose(long ptr);
}
