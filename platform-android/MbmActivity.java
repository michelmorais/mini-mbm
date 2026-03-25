/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| See LICENSE.md in the engine root for full license text.                                                               |
|-----------------------------------------------------------------------------------------------------------------------*/

package com.mini.mbm;

import android.app.NativeActivity;
import android.os.Bundle;
import android.os.Vibrator;
import android.content.Context;
import android.view.View;
import android.view.WindowManager;

/**
 * Thin NativeActivity wrapper for mini-mbm.
 *
 * The native engine (libmini-mbm.so) drives all rendering, input, and game logic.
 * This class only provides the small set of Android-specific helpers that cannot
 * be implemented in pure C++ without JNI:
 *   - vibrate(int ms)   — device vibration
 *   - getIdiom()        — locale/language string
 *   - onCallBackCommands(String, String) — C++ → Lua/scene callback bridge
 *
 * All file I/O, asset loading, and audio are handled natively (AAssetManager,
 * OpenSL ES) and do NOT require any calls back into Java.
 */
public class MbmActivity extends NativeActivity {

    static {
        // Load the engine shared library.  The name must match the CMake target name.
        System.loadLibrary("mini-mbm");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setImmersiveMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            setImmersiveMode();
        }
    }

    private void setImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
          | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
          | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
          | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
          | View.SYSTEM_UI_FLAG_FULLSCREEN
          | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    // -------------------------------------------------------------------------
    // Called from C++ via JNI to vibrate the device for the given duration.
    // -------------------------------------------------------------------------
    @SuppressWarnings("deprecation")
    public static void vibrate(Context context, int milliseconds) {
        try {
            Vibrator v = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
            if (v != null && v.hasVibrator()) {
                v.vibrate(milliseconds);
            }
        } catch (Exception e) {
            android.util.Log.e("MbmActivity", "vibrate failed: " + e.getMessage());
        }
    }

    // -------------------------------------------------------------------------
    // Returns the device locale as a string (e.g. "en", "pt", "fr").
    // Called from C++ via JNI to select the language for the UI.
    // -------------------------------------------------------------------------
    public static String getIdiom(Context context) {
        try {
            return context.getResources().getConfiguration().locale.getLanguage();
        } catch (Exception e) {
            return "en";
        }
    }

    // -------------------------------------------------------------------------
    // Callback bridge: C++ engine calls this when a command result is ready.
    // Override in your game Activity class to handle custom commands.
    // -------------------------------------------------------------------------
    public void onCallBackCommands(String param1, String param2) {
        // Default implementation is a no-op.
        // Override in your sub-class or handle in the engine's Lua onCallBackCommands.
    }
}
