package com.anilgames.test;

import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.View;

import com.google.androidgamesdk.GameActivity;

import java.util.Objects;

public class MainActivity extends GameActivity {
    static {
        System.loadLibrary("test");
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus) {
            hideSystemUi();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Objects.requireNonNull(getSupportActionBar()).hide(); // hide tab bar
        super.onCreate(savedInstanceState);
    }

    public void vibrate() {
        // Obtain the Vibrator system service
        Vibrator vibrator = (Vibrator) this.getSystemService(VIBRATOR_SERVICE);
        if (vibrator == null || !vibrator.hasVibrator()) {
            return; // No vibrator present, exit early
        }

        // Vibrate according to the API level
        vibrator.vibrate(VibrationEffect.createOneShot(200, VibrationEffect.DEFAULT_AMPLITUDE));
    }

    private void hideSystemUi() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
        );
    }
}