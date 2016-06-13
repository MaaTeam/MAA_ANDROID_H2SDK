package com.squareup.okhttp.internal;

import java.util.Locale;

import android.text.format.DateFormat;

import com.squareup.okhttp.Protocol;

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
  final long originLength;
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
    this.originLength = builder.originLength;
    this.cname = builder.cname;
  }

  public String getDestHost() {
    return destHost;
  }

  public String getResponseTime() {
    return DateFormat.format("yyyy-MM-dd kk:mm:ss", responseTimestamp)
        .toString();
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
    return String.format(Locale.US,
        "%d\t%s\t%s\t%s\t%s\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%s\t%d\t%s\t%d",
        responseTimestamp, relViaProxy, method, url, contentType, status,
        protocol, dns, conn, send, wait, recv, destHost, contentLength, cname,
        originLength);
  }

  @Override
  public String toString() {
    return String.format(Locale.US,
        "%d,%s,%s,%s,%s,%d,%s,%d,%d,%d,%d,%d,%s,%d,%s,%d", responseTimestamp,
        relViaProxy, method, url, contentType, status, protocol, dns, conn,
        send, wait, recv, destHost, contentLength, cname, originLength);
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
    private long originLength = -1;
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
      this.originLength = that.originLength;
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
      if (this.originLength == -1 && contentLength > 0)
        this.originLength = contentLength;
      return this;
    }

    public Builder setOriginalLength(long originLength) {
      this.originLength = originLength;
      return this;
    }

    public Builder setCname(String cname) {
      this.cname = cname;
      return this;
    }

    public Accesslog build() {
      return new Accesslog(this);
    }

    public static String getRelViaProxy(boolean useWsCname, boolean ishttps,
        boolean isproxy, boolean httpTohttps) {
      if (ishttps || httpTohttps) {
        return useWsCname ? (isproxy ? "DPS" : "PS") : "DS";
      } else {
        return useWsCname ? (isproxy ? "DP" : "P") : "D";
      }
    }
  }
}
