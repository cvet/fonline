#include <jni.h>
#include <stdlib.h>
#include "ipc.h"
#include "com_mittudev_ipc_Message.h"

JNIEXPORT jlong JNICALL Java_com_mittudev_ipc_Message_messageCreate
		(JNIEnv *env, jobject object, jbyteArray data){
	size_t len = (*env)->GetArrayLength(env, data);
	unsigned char* buf = malloc(len);
	(*env)->GetByteArrayRegion (env, data, 0, len, (jbyte*) buf);

	Message* msg = messageCreate(buf, len);

	return msg;
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Message_messageSetPID
		(JNIEnv *env, jobject object, jlong ptr, jint pid){
	Message* msg = (Message*) ptr;
	messageSetPID(msg, pid);
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Message_messageSetSubject
		(JNIEnv *env, jobject object, jlong ptr, jstring subject){
	Message* msg = (Message*) ptr;

	char* nativeSub = (*env)->GetStringUTFChars(env, subject, JNI_FALSE);

	messageSetSubject(msg, nativeSub);

	(*env)->ReleaseStringUTFChars(env, subject, nativeSub);
}

JNIEXPORT jbyteArray JNICALL Java_com_mittudev_ipc_Message_messageGetData
		(JNIEnv *env, jobject object, jlong ptr){
	Message* msg = (Message*) ptr;

	jbyteArray bytes = (*env)->NewByteArray(env, msg->len);
	(*env)->SetByteArrayRegion(env, bytes, 0, msg->len, msg->data);

	return bytes;
}

JNIEXPORT jstring JNICALL Java_com_mittudev_ipc_Message_messageGetSubject
		(JNIEnv *env, jobject object, jlong ptr){
	Message* msg = (Message*) ptr;

	if(msg->type != CONN_TYPE_SUB){
		return NULL;
	}

	jstring str = (*env)->NewStringUTF(env, msg->subject);
	return str;
}

JNIEXPORT jlong JNICALL Java_com_mittudev_ipc_Message_messageGetPID
		(JNIEnv *env, jobject object, jlong ptr){
	Message* msg = (Message*) ptr;
	return msg->pid;
}

JNIEXPORT jint JNICALL Java_com_mittudev_ipc_Message_messageGetType
		(JNIEnv *env, jobject object, jlong ptr){
	Message* msg = (Message*) ptr;
	return msg->type;
}

JNIEXPORT void JNICALL Java_com_mittudev_ipc_Message_messageDestroy
		(JNIEnv *env, jobject object, jlong ptr){
	Message* msg = (Message*) ptr;

	free(msg->data);
	messageDestroy(msg);
}
