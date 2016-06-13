package com.squareup.okhttp;

import java.io.File;
import java.io.FileNotFoundException;

import android.content.Context;
import android.text.TextUtils;

import com.squareup.okhttp.internal.AccesslogDispatcher;
import com.squareup.okhttp.internal.AccesslogReader;
import com.squareup.okhttp.internal.HttpProxy;
import com.squareup.okhttp.internal.NetworkBroadcastReceiver;
import com.squareup.okhttp.internal.WebviewProxy;

public class WebviewHandler implements NetworkBroadcastReceiver.Listener {
  final HttpProxy httpProxy;
  final AccesslogReader accesslogReader;
  final NetworkBroadcastReceiver receiver;
  
  private WebviewHandler(HttpProxy httpProxy, AccesslogReader accesslogReader,
      NetworkBroadcastReceiver receiver) {
    if (httpProxy == null) {
      throw new IllegalArgumentException("httpProxy can not be null");
    }
    
    this.httpProxy = httpProxy;
    this.accesslogReader = accesslogReader;
    this.receiver = receiver;
  }
  
  public static WebviewHandler get(AccesslogDispatcher accesslogDispatcher, 
      Context context, MaaPlus.Configuration configuration) {
    String accesslogPath = "";
    if (TextUtils.isEmpty(configuration.accesslogPath)) {
      accesslogPath = new File(context.getFilesDir(), "h2access_webview.log").getAbsolutePath();
    } else {
      accesslogPath = configuration.accesslogPath + "_webview";
    }
    
    AccesslogReader accesslogReader = null;
    if (configuration.accesslogReportEnabled) {
      try {
        accesslogReader = new AccesslogReader(accesslogPath, accesslogDispatcher);
      } catch (FileNotFoundException e) {
        e.printStackTrace();
      }
    }

    HttpProxy httpProxy = new HttpProxy();
    httpProxy.setAccesslogPath(accesslogPath);
    
    NetworkBroadcastReceiver receiver = new NetworkBroadcastReceiver(context);
    receiver.register();

    return new WebviewHandler(httpProxy, accesslogReader, receiver);
  }
  
  public void start() {
    receiver.setListener(this);
    httpProxy.start();
  }
  
  public boolean isStarted() {
    return httpProxy.isStarted();
  }
  
  public void setProxy(Context context) {
    if (httpProxy.isStarted() && (httpProxy.getLocalPort() != -1)) {
      WebviewProxy.setProxy(context, HttpProxy.LOCAL_HOST, httpProxy.getLocalPort());
    }
    
    if (accesslogReader != null) {
      accesslogReader.start();
    }
  }

  @Override
  public void onNetChanged() {
    if (httpProxy.isStarted()) {
      httpProxy.netChange();
    }
  }
}