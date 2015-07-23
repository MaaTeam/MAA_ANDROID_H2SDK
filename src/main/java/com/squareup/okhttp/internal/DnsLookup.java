package com.squareup.okhttp.internal;

import java.util.HashMap;

//import org.xbill.DNS.CNAMERecord;
//import org.xbill.DNS.Lookup;
//import org.xbill.DNS.Record;
//import org.xbill.DNS.TextParseException;
//import org.xbill.DNS.Type;

import android.text.TextUtils;

public class DnsLookup {
  private static int MAX_CACHE_TIME = 600; // seconds
  private static HashMap<String,Name> cacheMap = new HashMap<String,Name>();
  
  static {
    System.loadLibrary("cname");
  }
  
  public static native String getHostCname(String host);
  
  public static String getCNAMEByHost(String host) {
    Name name = cacheMap.get(host);
    if (name != null) {
      if (expired(name.expired)) {
        cacheMap.remove(host);
      } else {
        return name.cname;
      }
    }
    
    long startMs = System.currentTimeMillis();
    String cname = getHostCname(host);
    long endMs = System.currentTimeMillis();
    System.out.println("lookup cname use: " + (endMs - startMs) + " ms");
    System.out.println("lookup cname: " + cname);

    if (!TextUtils.isEmpty(cname)) {
      cacheMap.put(host, new Name(cname));
      return cname;
    }
    
//      try {
//        Lookup lookup = new Lookup(host, Type.CNAME);
//        Record[] records = lookup.run();
//
//        if (lookup.getResult() == Lookup.SUCCESSFUL) {
//          for (int i = 0; i < records.length; ++i) {
//            if (records[i] instanceof CNAMERecord) {
//              cname = ((CNAMERecord) records[i]).getAlias().toString();
//              System.out.println("CNAMERecord : " + cname);
//              return cname;
//            }
//          }
//        } else {
//          System.out.println(host + " no use cname");
//        }
//      } catch (TextParseException e) {
//        e.printStackTrace();
//      } finally {					
//        long endMs = System.currentTimeMillis();
//        System.out.println("lookup cname use: " + (endMs - startMs) + " ms");
//      }

      return null;
    }
  
  
  private static boolean expired(long expired) {
    long current = System.currentTimeMillis() / 1000;
    return current > expired;
  }
  
  private static class Name {
    public String cname;
    public long expired;
    public Name(String cname) {
      this.cname = cname;
      this.expired = System.currentTimeMillis() / 1000 + MAX_CACHE_TIME;
    }
  }
}
