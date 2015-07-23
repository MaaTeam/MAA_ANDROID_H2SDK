package com.squareup.okhttp;

public final class CNCConnectionSpec {
  final boolean httpTohttps;
  final boolean h2DirectOverTcp;
  final boolean h2DirectOverTls;
  final boolean useWsAccelerate;

  public CNCConnectionSpec(boolean useWsAccelerate, boolean httpTohttps, 
      boolean h2DirectOverTcp, boolean h2DirectOverTls) {
    this.useWsAccelerate = useWsAccelerate;
    this.httpTohttps = httpTohttps;
    this.h2DirectOverTcp = h2DirectOverTcp;
    this.h2DirectOverTls = h2DirectOverTls;
  }
   
  public boolean isHttpTohttps() {
    return httpTohttps;
  }
  
  public boolean isH2DirectOverTcp() {
    return h2DirectOverTcp;
  }

  public boolean isH2DirectOverTls() {
    return h2DirectOverTls;
  }
  
  public boolean isUseWsAccelerate() {
    return useWsAccelerate;
  }

  @Override public boolean equals(Object obj) {
    if (obj instanceof CNCConnectionSpec) {
      CNCConnectionSpec other = (CNCConnectionSpec) obj;
      return httpTohttps == other.httpTohttps;
    }
    return false;
  }
}
