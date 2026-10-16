package org.outland.game;
import android.app.NativeActivity;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.View;
import android.view.WindowManager;
/** Window/lifecycle boundary, the vibrator and the Wi-Fi multicast lock; all gameplay and touch input are native. */
public final class GameActivity extends NativeActivity {
    // Many phones drop broadcast packets to save power unless an app holds this: LAN PLAY's
    // game announcements need it while the game is open.
    private WifiManager.MulticastLock lanLock;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams params=getWindow().getAttributes();
            params.layoutInDisplayCutoutMode=WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(params);
        }
        immersive();
    }
    @Override protected void onResume() {
        super.onResume();
        try {
            if (lanLock == null) {
                WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);
                if (wifi != null) {lanLock = wifi.createMulticastLock("outland-lan"); lanLock.setReferenceCounted(false);}
            }
            if (lanLock != null) lanLock.acquire();
        } catch (Throwable ignored) {
        }
    }
    @Override protected void onPause() {
        try {
            if (lanLock != null && lanLock.isHeld()) lanLock.release();
        } catch (Throwable ignored) {
        }
        super.onPause();
    }
    private void immersive() {
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }
    /** Called from native code (platform::vibrate) for shots, hits and crashes. */
    public void vibrate(int milliseconds, int amplitude) {
        try {
            Vibrator vibrator = (Vibrator) getSystemService(VIBRATOR_SERVICE);
            if (vibrator == null || !vibrator.hasVibrator()) return;
            int force = vibrator.hasAmplitudeControl() ? Math.max(1, Math.min(255, amplitude)) : VibrationEffect.DEFAULT_AMPLITUDE;
            vibrator.vibrate(VibrationEffect.createOneShot(milliseconds, force));
        } catch (Throwable ignored) {
        }
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) immersive();
    }
}
