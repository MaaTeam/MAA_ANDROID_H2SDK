#include <jni.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>  
#include <string.h>
JavaVM* gJavaVM_ptr = NULL;

#ifdef __cplusplus
extern "C" {
#endif 

JNIEXPORT jstring Java_com_squareup_okhttp_internal_DnsLookup_getHostCname(JNIEnv* env, jobject thiz, jstring host);

#ifdef __cplusplus
}
#endif 

JNIEnv* getEnv(bool* attached) {
  JNIEnv* env = NULL;
  *attached = false;
  int ret = gJavaVM_ptr->GetEnv((void**)&env, JNI_VERSION_1_4);
  if (ret == JNI_EDETACHED) {
    if (0 != gJavaVM_ptr->AttachCurrentThread(&env, NULL)) {
      return NULL;
    }
    *attached = true;
    return env;
  }

  if (ret != JNI_OK) {
    return NULL;
  }

  return env;
}

void releaseEnv(bool attached){
  if (attached)
    gJavaVM_ptr->DetachCurrentThread();
}


static void convert_jstring_to_string(JNIEnv* env, const jstring jstr, char* str, int len)
{
  if(!jstr) {
    return;
  }
  
  const char* ptr = env->GetStringUTFChars(jstr, NULL);
  if (!ptr) return;

  //str.assign(ptr);
  strncpy(str, ptr, len);
  env->ReleaseStringUTFChars(jstr, ptr);
}

jstring convert_cstr_to_jstring(JNIEnv* env, const char* str)
{
  jstring str_result = NULL;
  jclass strClass = env->FindClass("java/lang/String");
  jmethodID ctorID = env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
  jbyteArray bytes = env->NewByteArray(strlen(str));
  env->SetByteArrayRegion(bytes, 0, strlen(str), (jbyte*)str);
  jstring encoding = env->NewStringUTF("utf-8");

  str_result = (jstring)env->NewObject(strClass, ctorID, bytes, encoding);
  env->DeleteLocalRef(bytes);
  env->DeleteLocalRef(encoding);
  env->DeleteLocalRef(strClass);
  return str_result;
}



JNIEXPORT jstring Java_com_squareup_okhttp_internal_DnsLookup_getHostCname(JNIEnv* env, jobject thiz, jstring host)
{
  int cur_index = 0;
  char sz_buf[512] = {};
  char str_host[128];
  struct addrinfo *answer = NULL;
  struct addrinfo hint = {};
  int ret = 0;
  jstring str_res = NULL;
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_CANONNAME;

  convert_jstring_to_string(env, host, str_host, sizeof(str_host));
  ret = getaddrinfo(str_host, NULL, &hint, &answer);

  if(ret != 0) {
    goto out;
  }

  for(;answer != NULL; answer =answer->ai_next) {
    if(answer->ai_canonname && answer->ai_canonname[0]) {
      strncpy(sz_buf, answer->ai_canonname, sizeof(sz_buf));
      break;
    }
  }
  str_res =  env->NewStringUTF(sz_buf);
out:
  if(answer) {
    freeaddrinfo(answer);
  }

  return str_res;

}


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  JNIEnv* env = NULL;
  gJavaVM_ptr = vm;
  return JNI_VERSION_1_4;
}