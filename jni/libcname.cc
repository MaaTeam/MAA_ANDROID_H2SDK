#include <jni.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>  
#include <string.h>

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

extern "C" JNIEXPORT jstring Java_com_squareup_okhttp_internal_DnsLookup_getHostCname(JNIEnv* env, jobject thiz, jstring jhost)
{
  char buf[512] = {};
  char chost[128];
  struct addrinfo *answer = NULL;
  struct addrinfo hint = {};
  int ret = 0;
  jstring jcname = NULL;
  
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_CANONNAME;

  convert_jstring_to_string(env, jhost, chost, sizeof(chost));
  ret = getaddrinfo(chost, NULL, &hint, &answer);

  if(ret != 0)
    goto out;

  for(;answer != NULL; answer =answer->ai_next) {
    if(answer->ai_canonname && answer->ai_canonname[0]) {
      strncpy(buf, answer->ai_canonname, sizeof(buf));
      break;
    }
  }
  jcname =  env->NewStringUTF(buf);
  
out:
  if(answer) {
    freeaddrinfo(answer);
  }

  return jcname;

}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  return JNI_VERSION_1_4;
}