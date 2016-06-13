#include <jni.h>
#include <string.h>
#include "shrpx_log.h"

static void convert_jstring_to_string(JNIEnv* env, const jstring jstr, char* str, int len)
{
  if(jstr == NULL)
    return;
  
  const char* ptr = env->GetStringUTFChars(jstr, NULL);
  if (ptr == NULL)
    return;

  strncpy(str, ptr, len);
  env->ReleaseStringUTFChars(jstr, ptr);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
	//return -1;
  return JNI_VERSION_1_4;
}

extern int shrpx_init(int argc, char **argv);
extern int shrpx_start();
extern int shrpx_stop();
extern int shrpx_getBindPort();
extern int shrpx_onNetworkChange();

extern "C" JNIEXPORT int Java_com_squareup_okhttp_internal_HttpProxy_nginit(JNIEnv* env, jobject thiz, jstring jargs) {
	char args[255] = {};
	char* args_arr[255] = {};
	int count = 0;
	char* ptr = args;
	convert_jstring_to_string(env, jargs, args, sizeof(args));

	if(!args[0]) {
		return -1;
	}

	count = 2;
	args_arr[0] = (char*)"nghttpx";
	args_arr[1] = (char*)args;

	for(;;) {
		ptr = strchr(ptr, ' ');
		if(!ptr) {
			break;
		}
		*ptr = '\0';
		ptr++;

		if(*ptr == 0) {
			break;
		}

		if(*ptr == ' ') {
			continue;
		}
		args_arr[count] = ptr;
		count ++;
	}

	return shrpx_init(count, (char**)args_arr);
}


extern "C" JNIEXPORT int Java_com_squareup_okhttp_internal_HttpProxy_ngstart(JNIEnv* env, jobject thiz) {
	return shrpx_start();
}

extern "C" JNIEXPORT int Java_com_squareup_okhttp_internal_HttpProxy_getBindPort(JNIEnv* env, jobject thiz) {
	return shrpx_getBindPort();
}

extern "C" JNIEXPORT int Java_com_squareup_okhttp_internal_HttpProxy_onNetworkChange(JNIEnv* env, jobject thiz) {
	return shrpx_onNetworkChange();
}

extern "C" JNIEXPORT int Java_com_squareup_okhttp_internal_HttpProxy_ngstop(JNIEnv* env, jobject thiz) {
	return shrpx_stop();
}