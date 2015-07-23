package com.squareup.okhttp.internal;

import java.text.MessageFormat;

import com.squareup.okhttp.Protocol;

import android.text.format.DateFormat;

public class Accesslog {
  final String destHost;
  final long responseTimestamp;
  final String url;
  final int status;
  final String relViaProxy;
  final String protocol;
  final String method;
  final String contentType;
  final long dns;
  final long conn;
  final long send;
  final long wait;
  final long recv;
  final long contentLength;
  final String cname;
  
  public Accesslog(Builder builder) {
    this.destHost = builder.destHost;
    this.responseTimestamp = builder.responseTimestamp;
    this.url = builder.url;
    this.status = builder.status;
    this.relViaProxy = builder.relViaProxy;
    this.protocol = builder.protocol;
    this.method = builder.method;
    this.contentType = builder.contentType;
    this.dns = builder.dns;
    this.conn = builder.conn;
    this.send = builder.send;
    this.wait = builder.wait;
    this.recv = builder.recv;
    this.contentLength = builder.contentLength;
    this.cname = builder.cname;
  }
  
  public String getDestHost() {
    return destHost;
  }

  public String getResponseTime() {
    return DateFormat.format("yyyy-MM-dd kk:mm:ss", responseTimestamp).toString(); 
  }

  public String getUrl() {
    return url;
  }

  public int getStatus() {
    return status;
  }

  public String getRelViaProxy() {
    return relViaProxy;
  }

  public String getProtocol() {
    return protocol;
  }

  public String getMethod() {
    return method;
  }

  public String getContentType() {
    return contentType;
  }

  public long getDns() {
    return dns;
  }

  public long getConn() {
    return conn;
  }

  public long getSend() {
    return send;
  }

  public long getWait() {
    return wait;
  }

  public long getRecv() {
    return recv;
  }

  public long getContentLength() {
    return contentLength;
  }

  public String getCname() {
    return cname;
  }

  public Builder newBuilder() {
    return new Builder(this);
  }
  
  public String toFormatString() {
    return MessageFormat.format(
        "{0,number,#}\t{1}\t{2}\t{3}\t{4}\t{5,number,#}" +
        "\t{6}\t{7,number,#}\t{8,number,#}\t{9,number,#}\t{10,number,#}\t{11,number,#}\t{12}\t{13,number,#}\t{14}", 
        responseTimestamp, relViaProxy, method, url, contentType, status, 
        protocol, dns, conn, send, wait, recv, destHost, contentLength, cname
       );
  }
  
  public String toString() {
    return MessageFormat.format(
        "responseTimestamp={0,number,#}" +
        ", relViaProxy={1}" +
        ", url={2}" +
        ", status={3}" +
        ", protocol={4}" +
        ", method={5}" +
        ", contentType={6}" +
        ", dns={7,number,#}" +
        ", conn={8,number,#}" +
        ", send={9,number,#}" +
        ", wait={10,number,#}" +
        ", recv={11,number,#}" +
        ", contentLength={12,number,#}" +
        ", destHost={13}" +
        ", cname={14}", 
        responseTimestamp, 
        relViaProxy, url, status, protocol, method, 
        contentType, dns, conn, send, wait, recv,
        contentLength, destHost, cname);
  }
  
  
  public static class Builder {
    private String destHost;
    private long responseTimestamp;
    private String url;
    private int status = -1;
    private String relViaProxy;
    private String protocol;
    private String method;
    private String contentType;
    private long dns = 0;
    private long conn = 0;
    private long send = 0;
    private long wait = 0;
    private long recv = 0;
    private long contentLength = -1;
    private String cname;
    
    public Builder() {
      
    }
    
    public Builder(Accesslog that) {
      this.destHost = that.destHost;
      this.responseTimestamp = that.responseTimestamp;
      this.url = that.url;
      this.status = that.status;
      this.relViaProxy = that.relViaProxy;
      this.protocol = that.protocol;
      this.method = that.method;
      this.contentType = that.contentType;
      this.dns = that.dns;
      this.conn = that.conn;
      this.send = that.send;
      this.wait = that.wait;
      this.recv = that.recv;
      this.contentLength = that.contentLength;
      this.cname = that.cname;
    }
    
    public Builder setDestHost(String destHost) {
      this.destHost = destHost;
      return this;
    }
    
    public Builder setResponseTime(long responseTimeMillis) {
      this.responseTimestamp = responseTimeMillis;
      return this;
    }
    
    public Builder setUrl(String url) {
      this.url = url;
      return this;
    }
    
    public Builder setStatus(int status) {
      this.status = status;
      return this;
    }
    
    public Builder setRelViaProxy(String relViaProxy) {
      this.relViaProxy = relViaProxy;
      return this;
    }

    public Builder setProtocol(Protocol protocol) {
      if (protocol == Protocol.HTTP_2) {
        this.protocol = "http/2.0";
      } else {
        this.protocol = protocol.toString();
      }
      return this;
    }

    public Builder setMethod(String method) {
      this.method = method;
      return this;
    }
    
    public Builder setContentType(String contentType) {
      this.contentType = contentType;
      return this;
    }
    
    public Builder setDns(long dns) {
      this.dns = dns;
      return this;
    }
    
    public Builder setConn(long conn) {
      this.conn = conn;
      return this;
    }
    
    public Builder setSend(long send) {
      this.send = send;
      return this;
    }
    
    public Builder setWait(long wait) {
      this.wait = wait;
      return this;
    }
    
    public Builder setRecv(long recv) {
      this.recv = recv;
      return this;
    }
    
    public Builder setContentLength(long contentLength) {
      this.contentLength = contentLength;
      return this;
    }
    
    public Builder setCname(String cname) {
      this.cname = cname;
      return this;
    }
    
    public Accesslog build() {
      return new Accesslog(this);
    }
    
    public static String getRelViaProxy(boolean useWsCname, 
        boolean ishttps, boolean isproxy, boolean httpTohttps) {
      if (ishttps || httpTohttps) {
        return useWsCname ? (isproxy ? "DPS" : "PS") : "DS";
      } else {
        return useWsCname ? (isproxy ? "DP" : "P") : "D";
      }
    }
  }
}
