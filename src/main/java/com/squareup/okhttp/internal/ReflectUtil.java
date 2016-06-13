
package com.squareup.okhttp.internal;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class ReflectUtil {
	public static Method getMethod(Class<?> clazz, String methodName, final Class<?>[] classes)
			throws Exception {
		Method method = null;
		try {
			method = clazz.getDeclaredMethod(methodName, classes);
		} catch (NoSuchMethodException e) {
			try {
				method = clazz.getMethod(methodName, classes);
			} catch (NoSuchMethodException ex) {
				if (clazz.getSuperclass() == null) {
					return method;
				} else {
					method = getMethod(clazz.getSuperclass(), methodName, classes);
				}
			}
		}
		return method;
	}

	/**
	 * @param obj 璋冩暣鏂规硶鐨勫璞�
	 * @param methodName 鏂规硶鍚�
	 * @param classes 鍙傛暟绫诲瀷鏁扮粍
	 * @param objects 鍙傛暟鏁扮粍
	 * @return 鏂规硶鐨勮繑鍥炲�
	 */
	public static Object invoke(final Object obj, final String methodName,
			final Class<?>[] classes, final Object[] objects) {
		try {
			Method method = getMethod(obj.getClass(), methodName, classes);
			method.setAccessible(true);// 璋冪敤private鏂规硶鐨勫叧閿竴鍙ヨ瘽
			return method.invoke(obj, objects);
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	public static Object invoke(final Object obj, final String methodName, final Class<?>[] classes) {
		return invoke(obj, methodName, classes, new Object[] {});
	}

	public static Object invoke(final Object obj, final String methodName) {
		return invoke(obj, methodName, new Class[] {}, new Object[] {});
	}

	public static Object getDeclaredField(Object obj, String name) throws SecurityException,
			NoSuchFieldException, IllegalArgumentException, IllegalAccessException {
		Field f = obj.getClass().getDeclaredField(name);
		f.setAccessible(true);
		Object out = f.get(obj);
		return out;
	}

	public static void setDeclaredField(Object obj, String name, Object value)
			throws SecurityException, NoSuchFieldException, IllegalArgumentException,
			IllegalAccessException {
		Field f = obj.getClass().getDeclaredField(name);
		f.setAccessible(true);
		f.set(obj, value);
	}

	public static Object invokeMethod(Object object, String methodName, Object[] params,
			Class<?>... types) throws Exception {
		Object out = null;
		Class<?> c = object instanceof Class ? (Class<?>) object : object.getClass();
		if (types != null) {
			Method method = c.getMethod(methodName, types);
			out = method.invoke(object, params);
		} else {
			Method method = c.getMethod(methodName);
			out = method.invoke(object);
		}
		return out;
	}
}
