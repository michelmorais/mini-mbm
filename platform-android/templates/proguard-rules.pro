# Add project-specific ProGuard rules here.
# NativeActivity applications have minimal Java code, so obfuscation risks are low.

# Keep NativeActivity and MbmActivity so that the activity class name is preserved
# and the system can launch it by the name declared in AndroidManifest.xml.
-keep public class com.mini.mbm.MbmActivity { *; }
-keep public class android.app.NativeActivity { *; }

# Keep JNI-accessible methods on MbmActivity
-keepclassmembers class com.mini.mbm.MbmActivity {
    public static void vibrate(android.content.Context, int);
    public static java.lang.String getIdiom(android.content.Context);
    public void onCallBackCommands(java.lang.String, java.lang.String);
}
