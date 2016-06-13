package com.squareup.okhttp.internal;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class AccesslogReader {
  private static final int MAX_READ_COUNT = 10;
  private static final int PERIOD_MS = 2000; // msec
  private final AccesslogDispatcher accesslogDispatcher;
  private final RandomAccessFile raf;
  private final ArrayList<String> logs;
  private ScheduledThreadPoolExecutor executor = null;
  private Future<?> future;
  private int seekPos;
  private boolean started;
  
  public AccesslogReader(String filePath, AccesslogDispatcher accesslogDispatcher) 
      throws FileNotFoundException {
    this.raf = new RandomAccessFile(filePath, "r");
    this.accesslogDispatcher = accesslogDispatcher;
    this.seekPos = 0;
    this.logs = new ArrayList<String>(MAX_READ_COUNT);
    this.started = false;
    
    this.executor = new ScheduledThreadPoolExecutor(1);
    this.executor.setExecuteExistingDelayedTasksAfterShutdownPolicy(false);
    this.executor.setContinueExistingPeriodicTasksAfterShutdownPolicy(false);
  }

  public void start() {
    if (this.started) return;
    
    this.future = this.executor.scheduleAtFixedRate(new SchedualReadTask(), 0, PERIOD_MS,
        TimeUnit.MILLISECONDS);
    this.started = true;
  }
  
  public void stop() {
    if (this.future != null) {
      this.future.cancel(true);
    }
    
    this.started = false;
  }
  
  private synchronized void onReadTimeout() {
    tryReadAccesslog();
    
    if (logs.isEmpty()) return;
    
    for (String log : logs) {
      this.accesslogDispatcher.addAccesslog(log, true);
    }
    logs.clear();
  }
  
  private void tryReadAccesslog() {
    try {
      raf.seek(seekPos);
      
      String line;
      int numlines = 0;
      while(numlines < MAX_READ_COUNT 
          && (line = raf.readLine()) != null) {
        logs.add(line);
        seekPos += (line.length() + 1);
        ++numlines;
      }
    } catch (IOException e) {
      e.printStackTrace();
    }
  }
  
  private class SchedualReadTask implements Runnable {

    @Override
    public void run() {
      onReadTimeout();
    }
    
  }
}
