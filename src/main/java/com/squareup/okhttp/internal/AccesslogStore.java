package com.squareup.okhttp.internal;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

public class AccesslogStore {
  private final int maxCount;
  private final FileWriter writer;
  private int count;
  
  public AccesslogStore(File storeFile, int maxCount) throws IOException {
    this.writer = new FileWriter(storeFile);
    this.maxCount = maxCount;
    this.count = 0;
  }

  public void store(String accesslog) {
    if (count >= maxCount) return;
    
    try {
      writer.write(accesslog);
      writer.write("\n");
      writer.flush();
      ++count;
    } catch (IOException e) {
      e.printStackTrace();
    }
    
    if (count >= maxCount) {
      try {
        writer.close();
      } catch (IOException e) {
        e.printStackTrace();
      }
    }

  }
}
