package com.squareup.okhttp.internal;

import com.squareup.okhttp.MaaPlusLog;

import android.text.TextUtils;

public class HttpProxy implements Runnable {

  public static final String LOCAL_HOST = "127.0.0.1";
  public static final int LOCAL_PORT = 8123;

  private native int nginit(String args);

  private native int ngstart();

  private native int getBindPort();

  private native int onNetworkChange();

  private String args = "-p --frontend-frame-debug -L INFO --backend-no-tls";

  private boolean started = false;

  public boolean isStarted() {
    return started;
  }

  public int getLocalPort() {
    if (!this.started)
      return -1;

    return getBindPort();
  }

  public int netChange() {
    if (!this.started)
      return -1;

    return onNetworkChange();
  }

  public void setAccesslogPath(String path) {
    if (!TextUtils.isEmpty(path)) {
      this.args += " --accesslog-file=" + path;
    }
  }

  public void start() {
    if (this.started) {
      return;
    }

    new Thread(this).start();
  }

  @Override
  public void run() {
    MaaPlusLog.i("Thread starting");
    try {
      if (nginit(args) != 0) {
        started = false;
        return;
      }
      started = true;

      if (ngstart() != 0) {
        started = false;
        return;
      }

    } catch (Throwable e) {
      e.printStackTrace();
    } finally {
      MaaPlusLog.i("Thread stopped");
      started = false;
    }
    
  }

  static {
    try {
      System.loadLibrary("nghttpx");
    } catch (Throwable e) {
      e.printStackTrace();
    }
  }

}
