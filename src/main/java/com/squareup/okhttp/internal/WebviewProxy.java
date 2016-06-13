package com.squareup.okhttp.internal;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Map;

import com.squareup.okhttp.MaaPlusLog;

import android.content.Context;
import android.content.Intent;
import android.net.Proxy;
import android.os.Build;
import android.os.Parcelable;
import android.text.TextUtils;

public class WebviewProxy {
	
	private static final int PROXY_CHANGED = 193;

	/**
	 * resetProxy
	 * 
	 * @param ctx
	 * @throws Exception
	 */

	@SuppressWarnings("deprecation")
	public static void resetProxy(Context ctx) {
		boolean success = false;
		int sdkInt = Build.VERSION.SDK_INT;
		try {
			if (sdkInt >= Build.VERSION_CODES.LOLLIPOP) {
				success = resetLollipopWebviewProxy(ctx);
			} 
			else if (sdkInt >= Build.VERSION_CODES.KITKAT) {
				success = resetKitKatWebViewProxy(ctx);
			} 
			else if (sdkInt >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
				success = resetICSProxy(ctx);
			} 
			else {
				success = resetFroyoProxy(ctx);
			}
		} catch (Throwable t) {
		  MaaPlusLog.e("Reset webview proxy error", t);
		}

		MaaPlusLog.d("Reset webview proxy " + (success ? "success" : "failure"));

		// FIXME may be need more consider
		String host = android.net.Proxy.getDefaultHost();
		int port = android.net.Proxy.getDefaultPort();
		if (!TextUtils.isEmpty(host) && port != -1) {
		  MaaPlusLog.d("Reset default host and port");
			setProxy(ctx, host, port);
		}
	}

	/**
	 * setProxy
	 * 
	 * @param ctx Android ApplicationContext
	 * @param host
	 * @param port
	 * @return true if Proxy was successfully set
	 */
	public static boolean setProxy(Context ctx, String host, int port) {
		boolean success = false;
		int sdkInt = Build.VERSION.SDK_INT;
		try {
			if (sdkInt >= Build.VERSION_CODES.LOLLIPOP) {
				success = setLollipopWebviewProxy(ctx, host, port);
			} 
			else if (sdkInt >= Build.VERSION_CODES.KITKAT) {
				success = setKitKatWebViewProxy(ctx, host, port);
			} 
			else if (sdkInt >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
				success = setICSProxy(host, port);
			} 
			else {
				success = setFroyoProxy(ctx, host, port);
			}
		} catch (Throwable e) {
		  MaaPlusLog.e("Webview set proxy error", e);
		}
		
		MaaPlusLog.d("Webview set proxy " + (success ? "success" : "failure"));
		
		return success;
	}

	/**
	 * android sdk api >= 8
	 */

	/**
	 * @param ctx
	 * @param host
	 * @param port
	 * @return
	 * @throws Exception
	 */
	private static boolean setFroyoProxy(Context ctx, String host, int port) throws Exception {
		Object requestQueueObject = getRequestQueue(ctx);
		if (requestQueueObject != null) {
			
			Class<?>[] argtypes = new Class[]{String.class, int.class, String.class};
			Object[] args = new Object[] {host, port, "http"};
			Class<?> cls = Class.forName("org.apache.http.HttpHost");
			Constructor<?> constructor = cls.getDeclaredConstructor(argtypes);
			constructor.setAccessible(true);
			Object httpHost = constructor.newInstance(args);
			
//			HttpHost httpHost = new HttpHost(host, port, "http");
			ReflectUtil.setDeclaredField(requestQueueObject, "mProxyHost", httpHost);
			return true;
		}
		return false;
	}

	private static boolean resetFroyoProxy(Context ctx) throws Exception {
		Object requestQueueObject = getRequestQueue(ctx);
		if (requestQueueObject != null) {
			ReflectUtil.setDeclaredField(requestQueueObject, "mProxyHost", null);
			return true;
		}
		return false;
	}

	private static Object getRequestQueue(Context ctx) throws Exception {
		Object ret = null;
		Class<?> networkClass = Class.forName("android.webkit.Network");
		if (networkClass != null) {
			Object networkObj = ReflectUtil.invokeMethod(networkClass, "getInstance", new Object[] {
				ctx
			}, Context.class);
			if (networkObj != null) {
				ret = ReflectUtil.getDeclaredField(networkObj, "mRequestQueue");
			}
		}
		return ret;
	}

	/**
	 * android sdk api >= 14
	 */

	/**
	 * @param host
	 * @param port
	 * @return
	 * @throws ClassNotFoundException
	 * @throws NoSuchMethodException
	 * @throws IllegalArgumentException
	 * @throws InstantiationException
	 * @throws IllegalAccessException
	 * @throws InvocationTargetException
	 */

	private static boolean setICSProxy(String host, int port) throws ClassNotFoundException,
			NoSuchMethodException, IllegalArgumentException, InstantiationException,
			IllegalAccessException, InvocationTargetException {
		Class<?> webViewCoreClass = Class.forName("android.webkit.WebViewCore");
		Class<?> proxyPropertiesClass = Class.forName("android.net.ProxyProperties");
		if (webViewCoreClass != null && proxyPropertiesClass != null) {
			Method m = webViewCoreClass.getDeclaredMethod(
					"sendStaticMessage", 
					Integer.TYPE,
					Object.class);
			
			Constructor<?> c = proxyPropertiesClass.getConstructor(
					String.class, 
					Integer.TYPE,
					String.class);
			
			m.setAccessible(true);
			c.setAccessible(true);
			Object properties = c.newInstance(host, port, null);
			m.invoke(null, PROXY_CHANGED, properties);

			return true;
		}
		return false;
	}

	private static boolean resetICSProxy(Context ctx) throws ClassNotFoundException,
			NoSuchMethodException, SecurityException, IllegalAccessException,
			IllegalArgumentException, InvocationTargetException {
		Class<?> webViewCoreClass = Class.forName("android.webkit.WebViewCore");
		if (webViewCoreClass != null) {
			Method m = webViewCoreClass.getDeclaredMethod(
					"sendStaticMessage", 
					Integer.TYPE,
					Object.class);
			
			m.setAccessible(true);
			m.invoke(null, PROXY_CHANGED, null);
			return true;
		}
		return false;
	}

	/**
	 * android sdk api >= 19
	 */

	/**
	 * @param appContext: must be application context
	 * @param host
	 * @param port
	 * @return
	 * @throws ClassNotFoundException
	 * @throws NoSuchFieldException
	 * @throws SecurityException
	 * @throws IllegalArgumentException
	 * @throws IllegalAccessException
	 * @throws NoSuchMethodException
	 * @throws InvocationTargetException
	 * @throws InstantiationException
	 */
	private static boolean setKitKatWebViewProxy(Context appContext, String host, int port)
			throws ClassNotFoundException, NoSuchFieldException, SecurityException,
			IllegalArgumentException, IllegalAccessException, NoSuchMethodException,
			InvocationTargetException, InstantiationException {
		
		SystemProxy systemProxy = null;
		if (host != null) {
			systemProxy = SystemProxy.get();
			systemProxy.setHttpProxy(host, port);
		}

		try {
			Map<?, ?> receivers = getAppReceivers(appContext);
			for (Object receiverMap : receivers.values()) {
				for (Object rec : ((Map<?, ?>) receiverMap).keySet()) {
					Class<? extends Object> clazz = rec.getClass();
					if (clazz.getName().contains("ProxyChangeListener")) {
					  MaaPlusLog.d("found ProxyChangeListener");
						Method onReceiveMethod = clazz.getDeclaredMethod("onReceive", Context.class,
								Intent.class);
						Intent intent = new Intent(Proxy.PROXY_CHANGE_ACTION);
	
						/*********** 4.4.4 *************/
						final String CLASS_NAME = "android.net.ProxyProperties";
						Class<?> cls = Class.forName(CLASS_NAME);
						Constructor<?> constructor = cls.getConstructor(String.class, Integer.TYPE,
								String.class);
						constructor.setAccessible(true);
						Object proxyProperties = constructor.newInstance(host != null ? host : "",
								port, null);
						intent.putExtra("proxy", (Parcelable) proxyProperties);
						/*********** 4.4.4 *************/
	
						onReceiveMethod.invoke(rec, appContext, intent);
						return true;
					}
				}
			}
			return false;

		} finally {
			if (host != null && systemProxy != null) {
				systemProxy.resetHttpProxy();
			}
		}
	}

	private static boolean resetKitKatWebViewProxy(Context appContext)
			throws ClassNotFoundException, NoSuchFieldException, SecurityException,
			IllegalArgumentException, IllegalAccessException, NoSuchMethodException,
			InvocationTargetException, InstantiationException {
		return setKitKatWebViewProxy(appContext, null, 0);
	}

	/**
	 * sdk api >= 21
	 * 
	 * @param appContext
	 * @param host
	 * @param port
	 * @return
	 * @throws ClassNotFoundException
	 * @throws NoSuchFieldException
	 * @throws SecurityException
	 * @throws IllegalArgumentException
	 * @throws IllegalAccessException
	 * @throws NoSuchMethodException
	 * @throws InvocationTargetException
	 * @throws InstantiationException
	 */
	private static boolean setLollipopWebviewProxy(Context appContext, String host, int port)
			throws ClassNotFoundException, NoSuchFieldException, SecurityException,
			IllegalArgumentException, IllegalAccessException, NoSuchMethodException,
			InvocationTargetException, InstantiationException {
		
		SystemProxy systemProxy = null;
		if (host != null) {
			systemProxy = SystemProxy.get();
			systemProxy.setHttpProxy(host, port);
		}

		try {
			Map<?, ?> receivers = getAppReceivers(appContext);
			for (Object receiverMap : receivers.values()) {
				for (Object rec : ((Map<?, ?>) receiverMap).keySet()) {
					Class<? extends Object> clazz = rec.getClass();
					if (clazz.getName().contains("ProxyChangeListener")) {
					  MaaPlusLog.d("found ProxyChangeListener");
						Method onReceiveMethod = clazz.getDeclaredMethod("onReceive", Context.class,
								Intent.class);
						Intent intent = new Intent(Proxy.PROXY_CHANGE_ACTION);
	
						Class<?> cls = Class.forName("android.net.ProxyInfo");
						Method m = cls.getMethod("buildDirectProxy", new Class[] {
								String.class, Integer.TYPE
						});
						Object proxyInfo = m.invoke(cls, new Object[] {
								host != null ? host : "", port
						});
						intent.putExtra("android.intent.extra.PROXY_INFO", (Parcelable) proxyInfo);
						onReceiveMethod.invoke(rec, appContext, intent);
						return true;
					}
				}
			}
			return false;
			
		} finally {
			if (host != null && systemProxy != null) {
				systemProxy.resetHttpProxy();
			}			
		}
	}

	private static boolean resetLollipopWebviewProxy(Context appContext)
			throws ClassNotFoundException, NoSuchFieldException, SecurityException,
			IllegalArgumentException, IllegalAccessException, NoSuchMethodException,
			InvocationTargetException, InstantiationException {
		return setLollipopWebviewProxy(appContext, null, 0);
	}
	
	private static Map<?, ?> getAppReceivers(Context appContext) 
			throws ClassNotFoundException,NoSuchFieldException, 
			IllegalAccessException, IllegalArgumentException {
		Class<?> applictionCls = Class.forName("android.app.Application");
		Field loadedApkField = applictionCls.getDeclaredField("mLoadedApk");
		loadedApkField.setAccessible(true);
		Object loadedApk = loadedApkField.get(appContext);
		Class<?> loadedApkCls = Class.forName("android.app.LoadedApk");
		Field receiversField = loadedApkCls.getDeclaredField("mReceivers");
		receiversField.setAccessible(true);
		return (Map<?, ?>) receiversField.get(loadedApk);
	}
}
