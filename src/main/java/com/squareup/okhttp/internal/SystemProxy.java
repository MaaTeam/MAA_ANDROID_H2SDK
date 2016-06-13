package com.squareup.okhttp.internal;

public class SystemProxy {
	private static final String HTTP_PROXY_HOST = "http.proxyHost";
	private static final String HTTP_PROXY_PORT = "http.proxyPort";
	private static final String HTTPs_PROXY_HOST = "https.proxyHost";
	private static final String HTTPs_PROXY_PORT = "https.proxyPort";

	private final String defaultHttpHost, defaultHttpPort;
	private final String defaultHttpsHost, defaultHttpsPort;

	private SystemProxy() {
		defaultHttpHost = System.getProperty(HTTP_PROXY_HOST);
		defaultHttpPort = System.getProperty(HTTP_PROXY_PORT);
		defaultHttpsHost = System.getProperty(HTTPs_PROXY_HOST);
		defaultHttpsPort = System.getProperty(HTTPs_PROXY_PORT);		
	}
	
	public static SystemProxy get() {
		return new SystemProxy();
	}
	
	public void setHttpProxy(String host, int port) {
		System.setProperty(HTTP_PROXY_HOST, host);
		System.setProperty(HTTP_PROXY_PORT, "" + port);
		System.setProperty(HTTPs_PROXY_HOST, host);
		System.setProperty(HTTPs_PROXY_PORT, "" + port);		
	}
	
	public void resetHttpProxy() {
		resetValue(HTTP_PROXY_HOST, defaultHttpHost);
		resetValue(HTTP_PROXY_PORT, defaultHttpPort);
		resetValue(HTTPs_PROXY_HOST, defaultHttpsHost);
		resetValue(HTTPs_PROXY_PORT, defaultHttpsPort);
	}
	
	private static void resetValue(String key, String value) {
		if (value == null) {
			System.getProperties().remove(key);
		} else {
			System.setProperty(key, value);
		}
	}
}
