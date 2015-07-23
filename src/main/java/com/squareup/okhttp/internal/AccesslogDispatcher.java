package com.squareup.okhttp.internal;

import java.io.File;

import com.squareup.okhttp.MaaPlus;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;


public class AccesslogDispatcher {
  private static final int ACCESSLOG_ADD = 1;
  final DispatchThread dispatchThread;
  final Handler handler;
  final AccesslogReportor reportor;
  final AccesslogStore store;
  
  public static AccesslogDispatcher get(Context context, MaaPlus.Configuration configuration) {
    AccesslogReportor reportor = null;
    if (configuration.accesslogReportEnabled) {
      reportor = new AccesslogReportor(context, configuration.eachReportNum, configuration.totalReportNum);
    }
    
    File file;
    if (TextUtils.isEmpty(configuration.accesslogPath)) {
      file = context.getFileStreamPath("h2access.log");
    } else {
      file = new File(configuration.accesslogPath);
    }
    AccesslogStore store = new AccesslogStore(file);
    
    AccesslogDispatcher dispatcher = new AccesslogDispatcher(reportor, store);
    return dispatcher;
  }
  
  public AccesslogDispatcher(AccesslogReportor reportor, AccesslogStore store) {
    this.reportor = reportor;
    this.store = store;
    this.dispatchThread = new DispatchThread();
    this.dispatchThread.start();
    
    this.handler = new DispatchHandler(this.dispatchThread.getLooper(), this);
  }

  public void addAccesslog(Accesslog accesslog) {
    this.handler.sendMessage(this.handler.obtainMessage(ACCESSLOG_ADD, accesslog));
  }
  
  private void performAdd(Accesslog accesslog) {
    System.out.println(accesslog.toString());
    
    if (reportor != null) {
      reportor.addAccesslog(accesslog);
    }
    
    if (store != null) {
      store.store(accesslog);
    }
  }
  
  private static class DispatchThread extends HandlerThread {
    DispatchThread() {
      super("DispatchThread", android.os.Process.THREAD_PRIORITY_BACKGROUND);
    }
  }
  
  private static class DispatchHandler extends Handler {
    private AccesslogDispatcher dispatcher;
    public DispatchHandler(Looper looper, AccesslogDispatcher dispatcher) {
      super(looper);
      this.dispatcher = dispatcher;
    }
    
    public void handleMessage(final Message msg) {
      switch(msg.what) {
      case ACCESSLOG_ADD:
        Accesslog accesslog = (Accesslog) msg.obj;
        this.dispatcher.performAdd(accesslog);
        break;
      }
    }
  }
}
