package com.mini.mbm;
import android.util.Log;

//Load the native framework
public class MiniMbmEngine
{
	public static boolean needRestart = false;
	static
	{
		try
		{
			System.loadLibrary("mini-mbm");
		}
		catch (UnsatisfiedLinkError | Exception e)
		{
			Log.d("debug_java",e.getMessage());
		}
	}

	public static native void init(int width, int height,String absPath,String apkPath,int expectedWidth,int expectedHeight);

	public static native void loop();

	public static native void quit();
	
	public static native void onTouchDown(int key ,float x,float y);
	
	public static native void onTouchUp(int key ,float x,float y);
	
	public static native void onTouchMove(int key ,float x,float y);
	
	public static native void onTouchZoom(float zoom);
	
	public static native void onKeyDown(int key);
	
	public static native void onKeyUp(int key);

	public static native void onKeyDownJoystick(int player,int key);

	public static native void onKeyUpJoystick(int player,int key);

	public static native void onMoveJoystick(int player, float lx, float ly, float rx, float ry);

	public static native void onInfoDeviceJoystick(int player, int maxNumberButton,String strDeviceName,String extraInfo);
	
	public static native void streamStopped(int indexJNI);

	public static native boolean onRestoreDevice(int width,int height);
	
	public static native void onStop();

	public static native void onCallBackCommands(String param1,String param2);
	
	
}
