package com.squareup.okhttp.internal;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.GZIPOutputStream;

import okio.BufferedSink;
import okio.Okio;

import com.squareup.okhttp.MaaPlus;
import com.squareup.okhttp.MaaPlusLog;
import com.squareup.okhttp.MediaType;
import com.squareup.okhttp.MultipartBuilder;
import com.squareup.okhttp.RequestBody;
import com.squareup.okhttp.XXTEA;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.telephony.TelephonyManager;
import android.text.TextUtils;

public class AccesslogReportor {
  private static final String ENCRYPT_KEY = "0123456789abcdeffedcba9876543210";

  private final Context context;
  private final ArrayList<String> accesslogs;
  private final int eachReportNum;
  private final int totalReportNum;
  private int currentReportNum;
  private final ExecutorService executor;
    
  public AccesslogReportor(Context context, int eachReportNum, int totalReportNum) {
    this.context = context;
    this.eachReportNum = eachReportNum;
    this.totalReportNum = totalReportNum;
    this.currentReportNum = 0;
    
    this.accesslogs = new ArrayList<String>();
    this.executor = Executors.newCachedThreadPool();
  }
  
  public void addAccesslog(Accesslog accesslog) {
    addAccesslog(accesslog.toFormatString());
  }
  
  public void addAccesslog(String accesslog) {
    if (isStopped()) {
      if (MaaPlus.DEBUG) MaaPlusLog.d("============>report stopped");
      return;
    }
    accesslogs.add(accesslog);
    if (isReadyToReport()) {
      if (MaaPlus.DEBUG) MaaPlusLog.d("============>report accesslog");
      report(getReportData());
      currentReportNum += eachReportNum;
      accesslogs.clear();
    }
  }
  
  private boolean isReadyToReport() {
    return accesslogs.size() == eachReportNum;
  }
  
  private boolean isStopped() {
    return (totalReportNum <= currentReportNum);
  }
  
  private String getReportData() {
    ArrayList<String> accesslogs = this.accesslogs;
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < accesslogs.size(); ++i) {
      sb.append(accesslogs.get(i)).append("\n");
    }
    return sb.toString();
  }
  
  private void report(final String reportData) {
    AccesslogSender sender = new AccesslogSender(reportData);
    this.executor.submit(sender);
  }
   
  private class AccesslogSender implements Runnable {
    private static final String REPORT_URL = "http://collect.dsp.chinanetcenter.com/file";
    private final String sendData; 
    
    public AccesslogSender(String sendData) {
      this.sendData = sendData;
    }
    
    @Override
    public void run() {
      try {
        URL url = new URL(REPORT_URL);
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        
        connection.setDoOutput(true);
//        connection.setChunkedStreamingMode(0);
        connection.setConnectTimeout(10000);
        
        try {
          MultipartBuilder multipartBuilder = new MultipartBuilder();
          multipartBuilder.type(MultipartBuilder.FORM);
          
          final HashMap<String,String> paramMap = getParams();
          for (Map.Entry<String, String> entry : paramMap.entrySet()) {
            multipartBuilder.addFormDataPart(entry.getKey(), entry.getValue());
          }
          RequestBody filePartBody = RequestBody.create(
              MediaType.parse("application/octet-stream"), 
              gzip(sendData.getBytes()));
          multipartBuilder.addFormDataPart("filename", "h2accesslog.gzip", filePartBody);

          RequestBody requestBody = multipartBuilder.build();
          connection.setRequestProperty("Content-Type", requestBody.contentType().toString());

          OutputStream out = connection.getOutputStream();
          BufferedSink bufferedRequestBody = Okio.buffer(Okio.sink(out));
          requestBody.writeTo(bufferedRequestBody);
          bufferedRequestBody.flush();
          out.close();
          
          if (connection.getResponseCode() == 200) {
            if (MaaPlus.DEBUG) {
              MaaPlusLog.d("report success");
              
              InputStream is = connection.getInputStream();
              StringBuilder responseMessage = new StringBuilder();
              int len;
              byte[] buffer = new byte[256];
              while((len = is.read(buffer)) != -1) {
                responseMessage.append(new String(buffer, 0, len));
              }
              MaaPlusLog.d("response msg: " + responseMessage.toString());
            }
          } else {
            if (MaaPlus.DEBUG) MaaPlusLog.d("report failure");
          }
        } finally {
          connection.disconnect();
        }
        
      } catch (MalformedURLException e) {
        e.printStackTrace();
      } catch (IOException e) {
        e.printStackTrace();
      }
    }
    
    private HashMap<String,String> getParams() {
      String imei = "unknown";
      String imsi = "unknown";
      
      final TelephonyManager telephonyManager = 
          (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
      
      try {
        imei = telephonyManager.getDeviceId();
        if (TextUtils.isEmpty(imei)) {
          imei = "unknown";
        }
      } catch (Throwable e) {
      }
      
      try {
        imsi = telephonyManager.getSubscriberId();
        if (TextUtils.isEmpty(imsi)) {
          imsi = "unknown";
        }
      } catch (Throwable e) {
      }
      
      imei = XXTEA.encrypt(imei, ENCRYPT_KEY);
      imsi = XXTEA.encrypt(imsi, ENCRYPT_KEY);
      
      HashMap<String, String> paramMap = new HashMap<String, String>();
      paramMap.put("packageName", context.getPackageName());
      paramMap.put("maasi", imsi);
      paramMap.put("networkType", getNetworkType(context));
      paramMap.put("maaid", imei);
      paramMap.put("appVersion", getAppVerion(context));
      paramMap.put("sdkVersion", MaaPlus.VERSION);
      paramMap.put("timestamp", String.valueOf(System.currentTimeMillis()));
      paramMap.put("platform", "android/" + Build.VERSION.RELEASE);
      paramMap.put("type", "maa-h2");
      paramMap.put("codec", "gzip");
      return paramMap;
    }
    
    private String getAppVerion(Context context) {
      PackageManager packageManager = context.getPackageManager();

      String appVersion = "unknown";
      try {
        PackageInfo packageInfo = packageManager.getPackageInfo(context.getPackageName(), 0);
        if (packageInfo != null) {
          appVersion = packageInfo.versionName;
        }
      } catch (Throwable e) {
      }
      return appVersion;
    }
    
    public byte[] gzip(byte[] source) throws IOException {
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      GZIPOutputStream gzip = new GZIPOutputStream(baos);
      gzip.write(source);
      gzip.flush();
      gzip.close();
      if (baos.size() == 0) {
        return null;
      }
      return baos.toByteArray();
    }
    
    public String getNetworkType(Context context) {
      try {
        ConnectivityManager connectivityManager = 
            (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivityManager == null) {
          return "Unknown";
        }
        NetworkInfo activeNetInfo = connectivityManager.getActiveNetworkInfo();
        if (activeNetInfo == null) {
          return "None";
        }
        if (!activeNetInfo.isAvailable() || !activeNetInfo.isConnected()) {
          return "None";
        }
        
        String networkType = "Unknown";
        switch (activeNetInfo.getType()) {
          case ConnectivityManager.TYPE_WIFI:
            networkType = "WIFI";
            break;

          case ConnectivityManager.TYPE_MOBILE:
            switch (activeNetInfo.getSubtype()) {
              case TelephonyManager.NETWORK_TYPE_EDGE:
                networkType = "EDGE";
                break;
              case TelephonyManager.NETWORK_TYPE_GPRS:
                networkType = "GPRS";
                break;
              case TelephonyManager.NETWORK_TYPE_CDMA:
              case TelephonyManager.NETWORK_TYPE_1xRTT:
                networkType = "CDMA";
                break;
              case TelephonyManager.NETWORK_TYPE_EVDO_0:
              case TelephonyManager.NETWORK_TYPE_EVDO_A:
              case TelephonyManager.NETWORK_TYPE_EVDO_B:
              case TelephonyManager.NETWORK_TYPE_UMTS:
              case TelephonyManager.NETWORK_TYPE_HSDPA:
              case TelephonyManager.NETWORK_TYPE_HSUPA:
              case TelephonyManager.NETWORK_TYPE_HSPA:
              case TelephonyManager.NETWORK_TYPE_HSPAP:
              case 17:
                networkType = "3G";
                break;
              case TelephonyManager.NETWORK_TYPE_LTE:
                networkType = "LTE";
                break;
            }
        }
        return networkType;
      } catch (Throwable e) {
        return "Unknown";
      }
    }
  }
  
}
