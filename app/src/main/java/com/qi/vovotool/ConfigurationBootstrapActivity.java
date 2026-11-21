package com.qi.vovotool;

import android.app.Activity;
import android.os.Bundle;

/** Establishes the scoped configuration URI grant, then exits without displaying a window. */
public final class ConfigurationBootstrapActivity extends Activity {
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);
        Settings.grantGameAccess(this);
        RootProxyService.reconcile(this);
        android.util.Log.i("vovotool","Configuration bootstrap completed");
        finish();
        overridePendingTransition(0,0);
    }
}
