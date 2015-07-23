package com.squareup.okhttp.internal;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;


public class AccesslogStore {
  
  private static final int MAX_COUNT = 2000;
  private int count;
  private FileWriter writer;
  
  public AccesslogStore(File storeFile) {
    try {
      this.writer = new FileWriter(storeFile);
    } catch (IOException e) {
      e.printStackTrace();
    }
    this.count = 0;
  }

  public void store(Accesslog accesslog) {
    if (writer == null) return;
    try {
      writer.write(accesslog.toFormatString());
      writer.write("\n");
      writer.flush();
      ++count;
    } catch (IOException e) {
      e.printStackTrace();
    }
    
    if (count > MAX_COUNT) {
      try {
        writer.close();
      } catch (IOException e) {
        e.printStackTrace();
      } finally {
        writer = null;
      }
    }

  }
}
