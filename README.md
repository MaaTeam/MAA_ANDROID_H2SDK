
Overview
========

本SDK基于开源库OkHttp-2.3.0改造而成，使用此SDK结合网宿科技MAA移动加速服务平台，为Android上的APP应用提供HTTP/2网络加速服务。
	
Usage
=====	

In the first stage, you need to include these permissions in your `AndroidManifest.xml` file
```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.READ_PHONE_STATE" />
```
After that, import **com.squareup.okhttp** package in your packages folder. So now everything is ready to start.

Let's get started
=================

Get MaaPlus singleton instance
```java
MaaPlus maaPlus = MaaPlus.with(context);
```
to enable request transform from http to https
```java
maaPlus.withHttpToHttpsEnabled(true);
```
to handle accesslog you can use these methods
```java
maaPlus.withAccesslogReportEnabled(true);
maaPlus.withAccesslogReportPolicy(10, 100);
maaPlus.withAccesslogPath("/sdcard/h2access.log");
```
finally, start service
```java
maaPlus.start();
```

Start network request
=====================
Get OkHttpClient instance
```java
OkHttpClient okClient = MaaPlus.getOkHttpClient();
```

You can use OkHttpClient to handle http request

Example:

Get Request
```java
OkHttpClient okClient = MaaPlus.getOkHttpClient();

String run(String url) throws IOException {
  Request request = new Request.Builder()
      .url(url)
      .build();

  Response response = okClient.newCall(request).execute();
  return response.body().string();
}
```

Post Request
```java
public static final MediaType JSON
    = MediaType.parse("application/json; charset=utf-8");

OkHttpClient okClient = MaaPlus.getOkHttpClient();

String post(String url, String json) throws IOException {
  RequestBody body = RequestBody.create(JSON, json);
  Request request = new Request.Builder()
      .url(url)
      .post(body)
      .build();
  Response response = okClient.newCall(request).execute();
  return response.body().string();
}
```

----

You can do request like **HttpURLConnection**

Example:
```java
...
OkHttpClient okClient = MaaPlus.getOkHttpClient();
HttpURLConnection conn = OkUrlFactory(okClient).open(url);
...
```

----

You can do request like apache **HttpClient**

Example:
```java
...
HttpClient client = new OkApacheClient();
...
```

