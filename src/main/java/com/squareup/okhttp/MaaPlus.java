package com.squareup.okhttp;

import java.security.GeneralSecurityException;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocketFactory;

import com.squareup.okhttp.internal.Accesslog;
import com.squareup.okhttp.internal.AccesslogDispatcher;
import com.squareup.okhttp.internal.tls.OkHostnameVerifier;

import android.app.Application;
import android.content.Context;

public class MaaPlus {
  public static final String VERSION = "1.1.0";
  public static final int H2_HTTPS_PORT = 443;
  public static final int H2_DIRECT_OVER_TLS_PORT = 6443;
  public static final int H2_DIRECT_OVER_TCP_PORT = 6480;
  public static final boolean DEBUG = false;
  final static SSLSocketFactory sslSocketFactory = getDefaultSSLSocketFactory();
  final static HostnameVerifier hostnameVerifier = new DefaultHostnameVerifier();
  final static CertificatePinner certificatePinner = CertificatePinner.DEFAULT;
  private Context context;
  private AccesslogDispatcher dispatcher;
  final Configuration configuration;
  private static final OkHttpClient okHttpClient = new OkHttpClient();
  
  static MaaPlus singleton = null;
  
  private MaaPlus(Context context) {
    this.context = context;
    this.configuration = new Configuration();
  }
  
  public static OkHttpClient getOkHttpClient() {
    return okHttpClient;
  }
  
  public static MaaPlus with(Context context) {
    if (singleton == null) {
      synchronized (MaaPlus.class) {
        if (singleton == null) {
          singleton = new MaaPlus(appContext(context));
        }
      }
    }
    return singleton;
  }
  
  public MaaPlus withAccesslogReportEnabled(boolean enabled) {
    configuration.accesslogReportEnabled = enabled;
    return this;
  }

  public MaaPlus withAccesslogReportPolicy(int eachReportNum, int totalReportNum) {
    if (eachReportNum <= 0) eachReportNum = 10;
    if (totalReportNum <= 0) totalReportNum = 100;
    configuration.eachReportNum = eachReportNum;
    configuration.totalReportNum = totalReportNum;
    return this;
  }

  public MaaPlus withAccesslogPath(String path) {
    configuration.accesslogPath = path;
    return this;
  }
  
  public MaaPlus withHttpToHttpsEnabled(boolean enabled) {
    okHttpClient.setHttpToHttpsEnabled(enabled);
    return this;
  }
  
  public void start() {
    dispatcher = AccesslogDispatcher.get(context, configuration);
  }

  void addAccesslog(Accesslog accesslog) {
    dispatcher.addAccesslog(accesslog);
  }
  
  private static Context appContext(Context context) {
    if (context instanceof Application) {
      return context;
    }
    return context.getApplicationContext();
  }
  
  private static SSLSocketFactory getDefaultSSLSocketFactory() {  
    try {
      SSLContext sslContext = SSLContext.getInstance("TLS");
      sslContext.init(null, null, null);
      return sslContext.getSocketFactory();
    } catch (GeneralSecurityException e) {
      throw new AssertionError(); // The system has no TLS. Just give up.
    }
  }
  
  private static class DefaultHostnameVerifier implements HostnameVerifier {
    private static final String DEFAULT_DOMAIN = "wsngh2.chinanetcenter.com";

    @Override
    public boolean verify(String host, SSLSession session) {
      if (OkHostnameVerifier.INSTANCE.verify(host, session)) {
        return true;
      }
      return OkHostnameVerifier.INSTANCE.verify(DEFAULT_DOMAIN, session);
    }

  }
  
  public static class Configuration {
    public boolean accesslogReportEnabled = true;
    public String accesslogPath;
    public int eachReportNum = 0;
    public int totalReportNum = 0;
  }
}
