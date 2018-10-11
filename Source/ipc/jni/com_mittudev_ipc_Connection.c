#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include "ipc.h"
#include "com_mittudev_ipc_Connection.h"

JavaVM* jvm;
jobject cbs[50];
char* names[50];

int findName(char* name){
	int i;
	for (i = 0; i < 50; i++) {
		if (names[i] && strcmp(names[i], name) == 0){
			return i;
		}
	}
	return -1;
}

int findFree(){
	int i;
	for (i = 0; i < 50; i++) {
		if (names[i] == NULL){
			return i;
		}
	}
	return -1;
}

void callback(Message* msg){
	JNIEnv* env;
	int result = (*jvm)->GetEnv(jvm, (void**) &env, JNI_VERSION_1_8);
	if (result == JNI_EDETACHED){
		(*jvm)->AttachCurrentThread(jvm, (void**) &env, NULL);
	}

	//size_t* name_len = msg->data + msg->len;
	char* name = msg->data + msg->len + sizeof(size_t);
	int i = findName(name);
	jobject cb = cbs[i];

	jclass clazz = (*env)->GetObjectClass(env, cb);
	jmethodID mid = (*env)->GetMethodID(env, clazz, "onMessage", "(Lcom/mittudev/ipc/Message;)V");

	char* data = malloc(msg->len);
	memcpy(data, msg->data, msg->len);

	Message* copy = messageCreate(data, msg->len);
	if (msg->type == CONN_TYPE_SUB){
		messageSetSubject(copy, msg->subject);
	}else if (msg->type == CONN_TYPE_PID){
		messageSetPID(copy, msg->pid);
	}

	jclass msgClazz = (*env)->FindClass(env, "com/mittudev/ipc/Message");
	jmethodID constructor = (*env)->GetMethodID(env, msgClazz, "<init>", "(J)V");
	jobject msgobj = (*env)->NewObject(env, msgClazz, constructor, copy);


	(*env)->CallVoidMethod(env, cb, mid, msgobj);
	free(data);
	messageDestroy(copy);

	(*jvm)->DetachCurrentThread(jvm);
}


JNIEXPORT jlong JNICALL Java_com_mittudev_ipc_Connection_connectionCreate
		(JNIEnv *env, jclass class, jstring name, jint type){
	(*env)->GetJavaVM(env, &jvm);

	char* nativeString = (*env)->GetStringUTFChars(env, name, JNI_FALSE);

	Connection* conn = connectionCreate(nativeString, type);

	(*env)->ReleaseStringUTFChars(env, name, nativeString);

	return conn;
}

JNIEXPORT jlong JNICALL Java_com_mittudev_ipc_Connection_connectionConnect
		(JNIEnv *env, jclass class, jstring name, jint type){
	(*env)->GetJavaVM(env, &jvm);

	char* nativeString = (*env)->GetStringUTFChars(env, name, JNI_FALSE);

	Connection* conn = connectionConnect(nativeString, type);

	(*env)->ReleaseStringUTFChars(env, name, nativeString);

	return conn;
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionStartAutoDispatch
		(JNIEnv *env, jobject object, jlong ptr){
	Connection* conn = (Connection*) ptr;
	connectionStartAutoDispatch(conn);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionStopAutoDispatch
		(JNIEnv *env, jobject object, jlong ptr){
	Connection* conn = (Connection*) ptr;
	connectionStartAutoDispatch(conn);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionSetCallback
		(JNIEnv *env, jobject object, jlong ptr, jobject cb){
	Connection* conn = (Connection*) ptr;
	int i = findFree();
	cbs[i] = (*env)->NewGlobalRef(env, cb);
	names[i] = malloc(strlen(conn->name) + 1);
	memcpy(names[i], conn->name, strlen(conn->name) + 1);
	connectionSetCallback(conn, callback);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionRemoveCallback
		(JNIEnv *env, jobject object, jlong ptr){
	Connection* conn = (Connection*) ptr;

	int i = findName(conn->name);
	free(names[i]);
	(*env)->DeleteGlobalRef(env, cbs[i]);
	cbs[i] = NULL;
	connectionRemoveCallback(conn);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionSend
		(JNIEnv *env, jobject object, jlong ptr, jlong msgPtr){
	Connection* conn = (Connection*) ptr;
	Message* msg = (Message*) msgPtr;

	connectionSend(conn, msg);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionSubscribe
		(JNIEnv *env, jobject object, jlong ptr, jstring sub){
	Connection* conn = (Connection*) ptr;
	char* subject =  (*env)->GetStringUTFChars(env, sub, JNI_FALSE);

	connectionSubscribe(conn, subject);

	(*env)->ReleaseStringUTFChars(env, sub, subject);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionRemoveSubscription
		(JNIEnv *env, jobject object, jlong ptr, jstring sub){
	Connection* conn = (Connection*) ptr;
	char* subject =  (*env)->GetStringUTFChars(env, sub, JNI_FALSE);

	connectionRemoveSubscription(conn, subject);

	(*env)->ReleaseStringUTFChars(env, sub, subject);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionDestroy
		(JNIEnv *env, jobject object, jlong ptr){
	Connection* conn = (Connection*) ptr;
	connectionDestroy(conn);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Connection_connectionClose
		(JNIEnv *env, jobject object, jlong ptr){
	Connection* conn = (Connection*) ptr;
	connectionClose(conn);
}
