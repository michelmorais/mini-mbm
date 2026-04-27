/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| See LICENSE.md in the engine root for full license text.                                                               |
|-----------------------------------------------------------------------------------------------------------------------*/

package com.mini.mbm;

import android.app.NativeActivity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Vibrator;
import android.os.VibrationEffect;
import android.view.View;
import android.view.WindowManager;

import java.util.concurrent.CountDownLatch;

/**
 * Thin NativeActivity wrapper for mini-mbm.
 *
 * The native engine (libmini-mbm.so) drives all rendering, input, and game logic.
 * This class provides Android-specific helpers that cannot be implemented in pure
 * C++ without JNI, and exposes the doCommands bridge to Lua via OnDoCommands().
 *
 * Built-in commands handled by OnDoCommands():
 *   vibrate          — haptic feedback (param: milliseconds as string)
 *   getidiom         — returns device locale language code ("en", "pt", ...)
 *   getapilevel      — returns Android API level as string
 *   clipboard_read   — reads primary clipboard text; returns text or ""
 *   clipboard_write  — writes param to clipboard; returns ""
 *   openURL          — opens param URL in external browser; returns ""
 *   pickFile         — shows file picker; result returned async via onCallBackCommands("pickFile", path)
 *   share            — shares a file path (FileProvider) or plain text; returns "done"
 *
 * Override OnDoCommands() in your game Activity subclass to handle custom commands
 * (call super.OnDoCommands(cmd, param) as default).
 */
public class MbmActivity extends NativeActivity {

    static {
        // Load the engine shared library.  The name must match the CMake target name.
        System.loadLibrary("mini-mbm");
    }

    private static final int REQ_PICK_FILE = 9001;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Route hardware volume buttons to the media stream used by OpenSL ES.
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
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
    // doCommands bridge — called from C++ android_command_handler via JNI.
    // Returns a result string that is forwarded back to the Lua caller.
    // Override in your game Activity subclass for custom commands.
    // -------------------------------------------------------------------------
    public String OnDoCommands(String cmd, String param) {
        if (cmd == null) return "";
        switch (cmd.toLowerCase()) {

            case "vibrate": {
                try {
                    int ms = Integer.parseInt(param.trim());
                    Vibrator v = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
                    if (v != null && v.hasVibrator()) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            v.vibrate(VibrationEffect.createOneShot(ms,
                                      VibrationEffect.DEFAULT_AMPLITUDE));
                        } else {
                            v.vibrate(ms);
                        }
                    }
                } catch (Exception e) {
                    android.util.Log.e("MbmActivity", "vibrate: " + e.getMessage());
                }
                return "";
            }

            case "getidiom": {
                try {
                    return getResources().getConfiguration().locale.getLanguage();
                } catch (Exception e) {
                    return "en";
                }
            }

            case "getapilevel": {
                return String.valueOf(Build.VERSION.SDK_INT);
            }

            case "clipboard_read": {
                // Must run on the UI thread; block with a latch (max 500 ms).
                final String[] result = {""};
                final CountDownLatch latch = new CountDownLatch(1);
                new Handler(Looper.getMainLooper()).post(() -> {
                    try {
                        ClipboardManager cm = (ClipboardManager)
                            getSystemService(Context.CLIPBOARD_SERVICE);
                        if (cm != null && cm.hasPrimaryClip()) {
                            ClipData.Item item = cm.getPrimaryClip().getItemAt(0);
                            CharSequence text = item.getText();
                            if (text != null) result[0] = text.toString();
                        }
                    } catch (Exception e) {
                        android.util.Log.e("MbmActivity", "clipboard_read: " + e.getMessage());
                    } finally {
                        latch.countDown();
                    }
                });
                try { latch.await(500, java.util.concurrent.TimeUnit.MILLISECONDS); }
                catch (InterruptedException ignored) {}
                return result[0];
            }

            case "clipboard_write": {
                final String text = param != null ? param : "";
                new Handler(Looper.getMainLooper()).post(() -> {
                    try {
                        ClipboardManager cm = (ClipboardManager)
                            getSystemService(Context.CLIPBOARD_SERVICE);
                        if (cm != null)
                            cm.setPrimaryClip(ClipData.newPlainText("", text));
                    } catch (Exception e) {
                        android.util.Log.e("MbmActivity", "clipboard_write: " + e.getMessage());
                    }
                });
                return "";
            }

            case "openurl": {
                try {
                    Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(param));
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    startActivity(intent);
                } catch (Exception e) {
                    android.util.Log.e("MbmActivity", "openURL: " + e.getMessage());
                }
                return "";
            }

            case "pickfile": {
                try {
                    Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
                    String mimeType = (param != null && !param.isEmpty()) ? param : "*/*";
                    intent.setType(mimeType);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    startActivityForResult(intent, REQ_PICK_FILE);
                } catch (Exception e) {
                    android.util.Log.e("MbmActivity", "pickFile: " + e.getMessage());
                }
                return "";
            }

            case "share": {
                try {
                    Intent shareIntent;
                    if (param != null && param.startsWith("/")) {
                        // File path — use FileProvider to create a content URI.
                        java.io.File file = new java.io.File(param);
                        if (file.exists()) {
                            String authority = getPackageName() + ".fileprovider";
                            android.net.Uri uri = androidx.core.content.FileProvider
                                .getUriForFile(this, authority, file);
                            String mime = getContentResolver().getType(uri);
                            if (mime == null) mime = "application/octet-stream";
                            shareIntent = new Intent(Intent.ACTION_SEND);
                            shareIntent.setDataAndType(uri, mime);
                            shareIntent.putExtra(Intent.EXTRA_STREAM, uri);
                            shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                        } else {
                            return "";
                        }
                    } else {
                        // Plain text share.
                        shareIntent = new Intent(Intent.ACTION_SEND);
                        shareIntent.setType("text/plain");
                        shareIntent.putExtra(Intent.EXTRA_TEXT, param);
                    }
                    startActivity(Intent.createChooser(shareIntent, null));
                } catch (Exception e) {
                    android.util.Log.e("MbmActivity", "share: " + e.getMessage());
                }
                return "done";
            }

            default:
                return "";
        }
    }

    // -------------------------------------------------------------------------
    // Called by the system when startActivityForResult() finishes.
    // Routes the selected file path back to C++ via nativeOnCallBackCommands.
    // -------------------------------------------------------------------------
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_PICK_FILE) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                Uri uri = data.getData();
                String path = resolveUriToPath(uri);
                nativeOnCallBackCommands("pickFile", path != null ? path : uri.toString());
            } else {
                nativeOnCallBackCommands("pickFile", "");
            }
        }
    }

    private String resolveUriToPath(Uri uri) {
        // Try content resolver DATA column first (works for local file URIs).
        try (android.database.Cursor cursor = getContentResolver().query(
                uri, new String[]{android.provider.MediaStore.MediaColumns.DATA},
                null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String path = cursor.getString(0);
                if (path != null && !path.isEmpty()) return path;
            }
        } catch (Exception ignored) {}
        // Fall back to the URI string itself (content:// or file://).
        return uri.toString();
    }

    // -------------------------------------------------------------------------
    // Native method — implemented in main-native-activity.cpp.
    // Delivers async command results (e.g. pickFile path) to the Lua scene.
    // -------------------------------------------------------------------------
    public native void nativeOnCallBackCommands(String p1, String p2);

    // -------------------------------------------------------------------------
    // Callback bridge: C++ engine calls this when a command result is ready.
    // The default implementation forwards to the native Lua callback.
    // Override in your game Activity subclass for custom handling.
    // -------------------------------------------------------------------------
    public void onCallBackCommands(String param1, String param2) {
        nativeOnCallBackCommands(param1 != null ? param1 : "",
                                 param2 != null ? param2 : "");
    }
}
