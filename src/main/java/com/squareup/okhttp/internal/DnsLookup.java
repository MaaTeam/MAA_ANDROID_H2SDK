package com.squareup.okhttp.internal;

import java.util.HashMap;

import com.squareup.okhttp.MaaPlus;

import android.text.TextUtils;
import android.util.Log;

public class DnsLookup {
  private static final int MAX_CACHE_TIME = 600; // seconds
  private static final HashMap<String, Name> cacheMap = new HashMap<String, Name>();
  private static final String REFER_HOST = "wsngh2.chinanetcenter.com";
  private static String cnameSuffix = null;

  static {
    try {
      System.loadLibrary("cname");
    } catch (UnsatisfiedLinkError e) {
      Log.e("OkHttp", "Please copy libcname.so to your libs");
    }
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

    long startMs;
    if (MaaPlus.DEBUG) {
      startMs = System.currentTimeMillis();
    }
    String cname = getHostCname(host);
    if (MaaPlus.DEBUG) {
      long endMs = System.currentTimeMillis();
      System.out.println("lookup cname use: " + (endMs - startMs) + " ms");
      System.out.println(String.format("lookup %s cname: %s", host, cname));
    }

    if (!TextUtils.isEmpty(cname)) {
      cacheMap.put(host, new Name(cname));
      return cname;
    }
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
  
  public static boolean isUseWsCname(String cname) {
    if (cnameSuffix == null) {
      String referCname = getCNAMEByHost(REFER_HOST);
      cnameSuffix = getCnameMark(referCname);
    }
    if (!TextUtils.isEmpty(cname) && !TextUtils.isEmpty(cnameSuffix)) {
      return cname.endsWith(cnameSuffix);
    }
    return false;
  }
  
  private static String getCnameMark(String referCname) {
    if (!TextUtils.isEmpty(referCname)) {
      int pos = referCname.indexOf(".");
      if (pos != -1) return referCname.substring(pos);
    }
    return "";
  }
}
