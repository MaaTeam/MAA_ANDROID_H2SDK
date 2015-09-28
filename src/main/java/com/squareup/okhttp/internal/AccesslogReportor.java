package com.squareup.okhttp.internal;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.GZIPOutputStream;

import com.squareup.okhttp.MaaPlus;
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
  private final ArrayList<Accesslog> accesslogs = new ArrayList<Accesslog>();
  private final int eachReportNum;
  private final int totalReportNum;
  private int currentReportNum;
    
  public AccesslogReportor(Context context, int eachReportNum, int totalReportNum) {
    this.context = context;
    this.eachReportNum = eachReportNum;
    this.totalReportNum = totalReportNum;
    this.currentReportNum = 0;
  }
  
  public void addAccesslog(Accesslog accesslog) {
    if (isStopped()) {
      if (MaaPlus.DEBUG) System.out.println("============>report stopped");
      return;
    }
    accesslogs.add(accesslog);
    if (isReadyToReport()) {
      if (MaaPlus.DEBUG) System.out.println("============>report accesslog");
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
    ArrayList<Accesslog> accesslogs = this.accesslogs;
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < accesslogs.size(); ++i) {
      sb.append(accesslogs.get(i).toFormatString()).append("\n");
    }
    return sb.toString();
  }
  
  private void report(final String reportData) {
    AccesslogSender sender = new AccesslogSender(reportData);
    Thread senderThread = new Thread(sender);
    senderThread.start();
  }
   
  private class AccesslogSender implements Runnable {
    private static final String REPORT_URL = "http://collect.dsp.chinanetcenter.com/file";
    private final String sendData; 
    
    public AccesslogSender(String sendData) {
      this.sendData = sendData;
    }
    
    public void run() {
      try {
        URL url = new URL(REPORT_URL);
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        
        connection.setDoOutput(true);
        connection.setChunkedStreamingMode(0);
        connection.setConnectTimeout(10000);
        
        MultipartTool multipart = new MultipartTool();
        addFileParams(multipart);
        try {
          OutputStream out = connection.getOutputStream();
          multipart.writeTo(out);
          out.close();
          
          if (connection.getResponseCode() == 200) {
            if (MaaPlus.DEBUG) System.out.println("report success");
          } else {
            if (MaaPlus.DEBUG) System.out.println("report failure");
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
    
    private void addFileParams(MultipartTool multipart) throws IOException {
      HashMap<String,String> paramMap = getParams();
      if (MaaPlus.DEBUG) System.out.println(paramMap.toString());
      for (Map.Entry<String, String> entry : paramMap.entrySet()) {
        multipart.addPart(entry.getKey(), entry.getValue());
      }
      multipart.addPart("filename", "h2accesslog.gzip", gzip(sendData.getBytes()), true);      
    }
    
    private HashMap<String,String> getParams() {
      final TelephonyManager telephonyManager = 
          (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
      String imei = telephonyManager.getDeviceId();
      if (TextUtils.isEmpty(imei)) {
        imei = "unknown";
      }
      
      String imsi = telephonyManager.getSubscriberId();
      if (TextUtils.isEmpty(imsi)) {
        imsi = "Unknown";
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
      paramMap.put("type", "h2sdk");
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
      } catch (PackageManager.NameNotFoundException e) {
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
  
  private static class MultipartTool {
    private final String boundary = "---7d4a6d158c9";
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    boolean isSetFirst = false;
      
    public void addPart(final String key, final String value) {
      writeFirstBoundaryIfNeeds();
      try {
        out.write(("Content-Disposition: form-data; name=\"" + key + "\"\r\n\r\n")
            .getBytes());
        out.write(value.getBytes());
        out.write(("\r\n--" + boundary + "\r\n").getBytes());
      } catch (final IOException e) {
        e.printStackTrace();
      }
    }
    
    public void addPart(final String key, final String fileName,
        final byte[] data, final boolean isLast) {
      addPart(key, fileName, data, "application/octet-stream", isLast);
    }
    
    public void addPart(final String key, final String fileName,
        final byte[] data, String type, final boolean isLast) {
      writeFirstBoundaryIfNeeds();
      try {
        type = "Content-Type: " + type + "\r\n";
        out.write(("Content-Disposition: form-data; name=\"" + key
            + "\"; filename=\"" + fileName + "\"\r\n").getBytes());
        out.write(type.getBytes());
        out.write("Content-Transfer-Encoding: binary\r\n\r\n".getBytes());

        out.write(data);
        if (!isLast)
          out.write(("\r\n--" + boundary + "\r\n").getBytes());
        out.flush();
      } catch (final Exception e) {
        e.printStackTrace();
      }
    }
    
    public void writeTo(final OutputStream outstream) throws IOException {
      outstream.write(out.toByteArray());
    }
    
    public void writeFirstBoundaryIfNeeds() {
      if (!isSetFirst) {
        try {
          out.write(("--" + boundary + "\r\n").getBytes());
        } catch (final IOException e) {
          e.printStackTrace();
        }
      }

      isSetFirst = true;
    }
  }
}
