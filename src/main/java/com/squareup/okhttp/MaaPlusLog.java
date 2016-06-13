package com.squareup.okhttp;

import android.util.Log;

public class MaaPlusLog {
  public static void d(String msg) {
    Log.d(MaaPlus.TAG, msg);
  }

  public static void i(String msg) {
    Log.i(MaaPlus.TAG, msg);
  }

  public static void w(String msg) {
    Log.w(MaaPlus.TAG, msg);
  }

  public static void e(String msg) {
    Log.e(MaaPlus.TAG, msg);
  }

  public static void e(String msg, Throwable error) {
    Log.e(MaaPlus.TAG, msg, error);
  }
}
