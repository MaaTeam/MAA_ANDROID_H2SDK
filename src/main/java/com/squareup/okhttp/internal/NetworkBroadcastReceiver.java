package com.squareup.okhttp.internal;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

public class NetworkBroadcastReceiver extends BroadcastReceiver {
	private static final String CONNECTIVITY_CHANGE_ACTION = "android.net.conn.CONNECTIVITY_CHANGE";
  private final Context context;
  private Listener listener;

  public NetworkBroadcastReceiver(Context context) {
    this.context = context;
	}

  public interface Listener {
    void onNetChanged();
	}
	
  public void setListener(Listener l) {
    this.listener = l;
  }

  public void register() {
		IntentFilter filter = new IntentFilter();
		filter.addAction(CONNECTIVITY_CHANGE_ACTION);
		filter.setPriority(1000);
    context.registerReceiver(this, filter);
	}

  public void unregister() {
    context.unregisterReceiver(this);
	}

	@Override
	public void onReceive(Context context, Intent intent) {
		if (intent == null) {
			return;
		}
		String action = intent.getAction();
		if (action.equals(CONNECTIVITY_CHANGE_ACTION)) {
      if (listener != null)
        listener.onNetChanged();
		}
	}
}
