package com.squareup.okhttp.internal;

import java.io.File;
import java.io.IOException;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;

import com.squareup.okhttp.MaaPlus;
import com.squareup.okhttp.MaaPlusLog;

public class AccesslogDispatcher {
  private static final int MAX_STORE_COUNT = 2000;
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
    
    AccesslogStore store = null;
    try {
      store = new AccesslogStore(file, MAX_STORE_COUNT);
    } catch (IOException e) {
      e.printStackTrace();
    }
    
    AccesslogDispatcher dispatcher = new AccesslogDispatcher(reportor, store);
    return dispatcher;
  }
  
  public AccesslogDispatcher(AccesslogReportor reporter, AccesslogStore store) {
    this.reportor = reporter;
    this.store = store;
    this.dispatchThread = new DispatchThread();
    this.dispatchThread.start();
    
    this.handler = new DispatchHandler(this.dispatchThread.getLooper(), this);
  }

  public void addAccesslog(Accesslog accesslog) {
    addAccesslog(accesslog.toFormatString(), true);
  }
  
  public void addAccesslog(String accesslog, boolean needStore) {
    this.handler.obtainMessage(ACCESSLOG_ADD, needStore ? 1 : 0, 0, accesslog).sendToTarget();
  }
  
  private void performAdd(String accesslog, boolean needStore) {
    if (MaaPlus.DEBUG) MaaPlusLog.d(accesslog);
    
    if (reportor != null) {
      reportor.addAccesslog(accesslog);
    }
    
    if (needStore && store != null) {
      store.store(accesslog);
    }
  }
  
  private static class DispatchThread extends HandlerThread {
    DispatchThread() {
      super("DispatchThread", android.os.Process.THREAD_PRIORITY_BACKGROUND);
    }
  }
  
  private static class DispatchHandler extends Handler {
    private final AccesslogDispatcher dispatcher;
    public DispatchHandler(Looper looper, AccesslogDispatcher dispatcher) {
      super(looper);
      this.dispatcher = dispatcher;
    }
    
    @Override
    public void handleMessage(final Message msg) {
      switch(msg.what) {
      case ACCESSLOG_ADD:
        String accesslog = (String) msg.obj;
        boolean needStore = msg.arg1 == 1;
        this.dispatcher.performAdd(accesslog, needStore);
        break;
      }
    }
  }
}
