package com.mini.mbm;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.ContentUris;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.util.Log;
import android.util.SparseArray;
import android.view.InputDevice;// Storage Permissions
import android.view.ScaleGestureDetector;
import androidx.appcompat.app.AppCompatActivity;
import android.view.InputDevice.MotionRange;
import android.view.InputEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.graphics.Point;
import android.widget.Toast;
import androidx.appcompat.app.ActionBar;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import static android.Manifest.*;

public abstract class  MainJniEngine<MainActivity> extends AppCompatActivity
{
	private static final int REQUEST_CODE_ASK_READ_WRITE_SD_CARD = 22;
	private static final int REQUEST_CODE_ASK_FOR_VIBRATION = 23;
	ViewJniEngine view = null;
	/*Gesture*/
	private ScaleGestureDetector scaleDetector;
	private float scaleFactor = 1.f;
	private float lastScaleFactor = 1.f;

	/*Joystick*/
	private SparseArray<InputDeviceState> lsInputDevicesState;

	public String getMyVersionAPP()
	{
		// Different version of "versionName" force to replace all assets
		// This is useful when there is some update in the asset
		// In order to optimize ecah time that the app is started , the engine copy only once the asset to an area of usage,
		// changing the version of the code force to replace all assets
		// usually we do that when there is new version of it (different code / texture / lib) which needs to be updated
		
		try {
			PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
			return packageInfo.versionName;
		} catch (PackageManager.NameNotFoundException e) {
			Log.d("debug_java","getMyVersionAPP:"+ e.toString());
			return "v1.0";
		}
	}

	public abstract int getMaxSimultaneousStreams();


	/*
	*** In Lua side ***
	Local result = mbm:doCommands(“myCommand”,”Parameter”)
	*** In Java side ***
	@Override
	public String OnDoCommands(String key, String param)
	{
		if(key== “myCommand”)
			Log.d("debug_java", "My command:" + key +” Parameter:” + param);
		return "nop";
	}
	*/

	public abstract Point getExpectedSizeOfWindow();

	public abstract String OnDoCommands(String key, String param);

	public abstract void OnCreate(Bundle icicle);

	public abstract void OnResume();

	public abstract void OnStart();

	public abstract void OnPause();

	public abstract void OnStop();

	public abstract void OnDestroy();

	public abstract void OnActivityResult(int requestCode, int resultCode, Intent data);

	public abstract void OnPostCreate(Bundle savedInstanceState);

	//ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE; -> Horizontal
	//ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT -> Vertical
	public abstract int getScreenOrientation();

	@Override
	protected final void onCreate(Bundle icicle)
	{
		super.onCreate(icicle);

		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,WindowManager.LayoutParams.FLAG_FULLSCREEN);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		setSensorOrientation();

		if (view == null)
		{
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
			{
				hideSystemUI();
			}
			String version =this.getMyVersionAPP();
			if(version == null)
				version = "v1.0";
			Point sizeWindow = this.getExpectedSizeOfWindow();
			if(sizeWindow == null)
				sizeWindow = new Point(1024,768);
			view = new ViewJniEngine(this.getApplication(),
					this,
					version,
					Math.max(this.getMaxSimultaneousStreams(),5),sizeWindow.x,sizeWindow.y);
		}
		setContentView(view);
		scaleDetector = new ScaleGestureDetector(this, new ScaleListener());
		this.lsInputDevicesState = new SparseArray<>();
		memoryUP();
		try
		{
			this.OnCreate(icicle);
		}
		catch (Exception e)
		{
			Log.d("debug_java","onCreate:"+ e.toString());
		}
	}

	@Override
	protected void onPostCreate(Bundle savedInstanceState)
	{
		super.onPostCreate(savedInstanceState);
		this.OnPostCreate(savedInstanceState);
	}


	@Override
	protected final void onResume()
	{
		super.onResume();
		try
		{
			view.onResume();
			this.OnResume();
		}
		catch (Exception e)
		{
			Log.d("debug_java","onResume:"+ e.toString());
		}
	}

	@Override
	protected final void onStart()
	{
		super.onStart();
		try
		{
			this.OnStart();
		}
		catch (Exception e)
		{
			Log.d("debug_java","onStart:"+ e.toString());
		}
	}

	@Override
	protected final void onPause()
	{
		super.onPause();
		try
		{
			view.onPause();
			this.refresh();
			this.OnPause();
		}
		catch (Exception e)
		{
			Log.d("debug_java","onPause:"+ e.toString());
		}

	}

	@Override
	protected final void onStop()
	{
		super.onStop();
		try
		{
			this.OnStop();
		}
		catch (Exception e)
		{
			Log.d("debug_java","onStop:"+ e.toString());
		}
	}
	//-----------------------------------------------------------------
	@Override
	protected final void onDestroy()
	{
		super.onDestroy();
		try
		{
			MiniMbmEngine.needRestart = true;
			this.OnDestroy();
			if (!this.isFinishing())
				this.finish();
			android.os.Process.killProcess(android.os.Process.myPid());
		}
		catch (Exception e)
		{
			Log.d("debug_java","onDestroy:"+ e.toString());
		}

	}
	//-----------------------------------------------------------------
	private class ScaleListener extends ScaleGestureDetector.SimpleOnScaleGestureListener
	{
		@Override
		public final boolean onScale(ScaleGestureDetector detector)
		{
			scaleFactor *= detector.getScaleFactor();
			return true;
		}
	}

	public void refresh()
	{
		if (!this.isFinishing())
		{
			MiniMbmEngine.onStop();
			InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
			MiniMbmEngine.needRestart = true;
			TextureManagerJniEngine.release(mainActivity.gl10);
		}
	}

	@SuppressLint("NewApi")
	protected void memoryUP()
	{
		//To be added at XML: android:largeHeap="true"
		ActivityManager am = (ActivityManager) getApplicationContext().getSystemService(ACTIVITY_SERVICE);
		if(am != null)
		{
			final int current = am.getMemoryClass();
			final int more = am.getLargeMemoryClass();
			Log.i("debug_java","Heap current[" + current + "] ... more:["+ more + "]");
		}
	}

	/**
	 * @param function name to be called in LUA
	 * @param param param to be used on LUA function
	 *  Call this function if you want to call some function in lua from JAVA
	 *  eg.:
	 *  luaFunction("print","Hello from Java!")
	 */
	public void luaFunction(String function,String param)
	{
		view.renderer.onCallBackCommands(function,param);
	}

	@Override
	public final boolean dispatchKeyEvent(KeyEvent event)
	{
		InputDeviceState state = getInputDeviceState(event);
		if (state != null)
		{
			if(state.isJoystick)
			{
				final int keyCode = event.getKeyCode();
				int key = 0;
				switch (keyCode)
				{
					case KeyEvent.KEYCODE_BUTTON_A:key = 1 ;break;
					case KeyEvent.KEYCODE_BUTTON_B:key = 2 ;break;
					case KeyEvent.KEYCODE_BUTTON_C:key = 3 ;break;
					case KeyEvent.KEYCODE_BUTTON_X:key = 4 ;break;
					case KeyEvent.KEYCODE_BUTTON_Y:key = 5 ;break;
					case KeyEvent.KEYCODE_BUTTON_Z:key = 6 ;break;
					case KeyEvent.KEYCODE_BUTTON_L1:key = 7 ;break;
					case KeyEvent.KEYCODE_BUTTON_R1:key = 8 ;break;
					case KeyEvent.KEYCODE_BUTTON_L2:key = 9 ;break;
					case KeyEvent.KEYCODE_BUTTON_R2:key = 10 ;break;
					case KeyEvent.KEYCODE_BUTTON_THUMBL:key = 11 ;break;
					case KeyEvent.KEYCODE_BUTTON_THUMBR:key = 12 ;break;
					case KeyEvent.KEYCODE_BUTTON_START:key = 13 ;break;
					case KeyEvent.KEYCODE_BUTTON_SELECT:key = 14 ;break;
					case KeyEvent.KEYCODE_BUTTON_MODE:key = 15 ;break;
					case KeyEvent.KEYCODE_BUTTON_1:key = 16 ;break;
					case KeyEvent.KEYCODE_BUTTON_2:key = 17 ;break;
					case KeyEvent.KEYCODE_BUTTON_3:key = 18 ;break;
					case KeyEvent.KEYCODE_BUTTON_4:key = 19 ;break;
					case KeyEvent.KEYCODE_BUTTON_5:key = 20 ;break;
					case KeyEvent.KEYCODE_BUTTON_6:key = 21 ;break;
					case KeyEvent.KEYCODE_BUTTON_7:key = 22 ;break;
					case KeyEvent.KEYCODE_BUTTON_8:key = 23 ;break;
					case KeyEvent.KEYCODE_BUTTON_9:key = 24 ;break;
					case KeyEvent.KEYCODE_BUTTON_10:key = 25 ;break;
					case KeyEvent.KEYCODE_BUTTON_11:key = 26 ;break;
					case KeyEvent.KEYCODE_BUTTON_12:key = 27 ;break;
					case KeyEvent.KEYCODE_BUTTON_13:key = 28 ;break;
					case KeyEvent.KEYCODE_BUTTON_14:key = 29 ;break;
					case KeyEvent.KEYCODE_BUTTON_15:key = 30 ;break;
					case KeyEvent.KEYCODE_BUTTON_16:key = 31 ;break;
					case KeyEvent.KEYCODE_DPAD_UP:key = 32 ;break;
					case KeyEvent.KEYCODE_DPAD_DOWN:key = 33 ;break;
					case KeyEvent.KEYCODE_DPAD_LEFT:key = 34 ;break;
					case KeyEvent.KEYCODE_DPAD_RIGHT:key = 35 ;break;
					case KeyEvent.KEYCODE_DPAD_CENTER:key = 36 ;break;
					case KeyEvent.KEYCODE_SPACE:key = 37 ;break;
				}
				if(key != 0 )
				{
					switch (event.getAction())
					{
						case KeyEvent.ACTION_DOWN:
						{
							if (event.getRepeatCount() == 0)
								view.renderer.onKeyDownJoystick(1, key);
							return this.getRetKeyEvent(keyCode);
						}
						case KeyEvent.ACTION_UP:
						{
							if (event.getRepeatCount() == 0)
								view.renderer.onKeyUpJoystick(1, key);
							return this.getRetKeyEvent(keyCode);
						}
						default:
							return super.dispatchKeyEvent(event);
					}
				}
				else
				{
					switch (event.getAction())
					{
						case KeyEvent.ACTION_DOWN:
						{
							if (event.getRepeatCount() == 0)
								view.renderer.onKeyDownJoystick(1, key);
							return this.getRetKeyEvent(keyCode);
						}
						case KeyEvent.ACTION_UP:
						{
							if (event.getRepeatCount() == 0)
								view.renderer.onKeyUpJoystick(1, key);
							return this.getRetKeyEvent(keyCode);
						}
						default:

							return super.dispatchKeyEvent(event);
					}
				}
			}
			else
			{
				final int keyCode = event.getKeyCode();
				switch (event.getAction())
				{
					case KeyEvent.ACTION_DOWN:
					{
						if (event.getRepeatCount() == 0)
							view.renderer.onKeyDown(keyCode);
						return this.getRetKeyEvent(keyCode);
					}
					case KeyEvent.ACTION_UP:
					{
						if (event.getRepeatCount() == 0)
							view.renderer.onKeyUp(keyCode);
						return this.getRetKeyEvent(keyCode);
					}
					default:
						return super.dispatchKeyEvent(event);
				}
			}
		}
		return super.dispatchKeyEvent(event);
	}

	private boolean getRetKeyEvent(final int keyCode)
	{
		switch(keyCode)
		{
			case KeyEvent.KEYCODE_VOLUME_DOWN:
			case KeyEvent.KEYCODE_VOLUME_UP:
			case KeyEvent.KEYCODE_VOLUME_MUTE:
				return false;
			default:return true;
		}
	}

	private InputDeviceState getInputDeviceState(InputEvent event)
	{
		final int deviceId = event.getDeviceId();
		InputDeviceState state = this.lsInputDevicesState.get(deviceId);
		if (state == null)
		{
			final InputDevice device = event.getDevice();
			if (device == null)
			{
				return null;
			}
			state = new InputDeviceState(device);
			this.lsInputDevicesState.put(deviceId, state);
		}
		return state;
	}

	@Override
	public final boolean dispatchGenericMotionEvent(MotionEvent event)
	{
		final int action = event.getAction();
		if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0
				&& action == MotionEvent.ACTION_MOVE)
		{
			InputDeviceState state = getInputDeviceState(event);
			if(state != null)
				return state.onMoveJoystick(event);
		}
		return super.dispatchGenericMotionEvent(event);
	}

	@Override
	public final boolean onTouchEvent(MotionEvent event)
	{
		scaleDetector.onTouchEvent(event);
		final int key 		= event.getPointerCount();
		final float x 		= event.getX();
		final float y 		= event.getY();
		final int action 	= event.getAction() & MotionEvent.ACTION_MASK;
		switch(action)
		{
			case MotionEvent.ACTION_DOWN:
			case MotionEvent.ACTION_POINTER_DOWN:
			{
				view.renderer.onTouchDown(key,x, y);
			}
			break;
			case MotionEvent.ACTION_UP:
			case MotionEvent.ACTION_POINTER_UP:
			{
				view.renderer.onTouchUp(key,x, y);
			}
			break;
			case MotionEvent.ACTION_MOVE:
			{
				if (!scaleDetector.isInProgress())
				{
					view.renderer.onTouchMove(key,x, y);
				}
				else
				{
					if(scaleFactor > lastScaleFactor)
						view.renderer.onTouchZoom(1);
					else
						view.renderer.onTouchZoom(-1);
					lastScaleFactor = scaleFactor;
				}
			}
			break;
			default:
			{
				Log.d("debug_java","default key " + key + " action:" + action);
			}
		}
		return true;
	}
	@Override
	public final boolean onKeyDown(int keyCode, KeyEvent event)
	{
		view.renderer.onKeyDown(keyCode);//KEYCODE_BACK(voltar) menu(KeyEvent.KEYCODE_DPAD_CENTER == 23)
		return false;
	}
	@Override
	public final boolean onKeyUp(int keyCode, KeyEvent event)
	{
		view.renderer.onKeyUp(keyCode);
		return false;
	}

	private void setSensorOrientation()
	{
		try
		{
			// If you remove android:screenOrientation="landscape" from AndroidManifest.xml screenOrientation become -1 which is none of orientation below
			// you need to set the orientation here
			int screenOrientation = getPackageManager().getPackageInfo(getPackageName(), PackageManager.GET_ACTIVITIES).activities[0].screenOrientation;
			if (screenOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)
				setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
			else if (screenOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT)
				setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT);
			else
				setRequestedOrientation(getScreenOrientation());

		}
		catch (Exception e)
		{
			e.printStackTrace();
		}
	}

	protected void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		if (requestCode == DoCommandsJniEngine.SELECT_PICTURE)
		{
			if(resultCode == Activity.RESULT_OK)
			{
				try
				{
					InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
					Uri uri = data.getData();
					final String filenamePathURI = this.getPathFromUri(uri);
					if(filenamePathURI != null)
					{
						this.view.renderer.onCallBackCommands(DoCommandsJniEngine.strFunctionCallBack, this.addUri(uri,filenamePathURI));
					}
					else
					{
						String[] filePathColumn = {MediaStore.Images.Media.DATA};
						Cursor cursor = getContentResolver().query(uri,
								filePathColumn, null, null, null);
						if (cursor == null)
						{
							String picturePath = uri.getPath();
							if (picturePath == null)
							{
								this.view.renderer.onCallBackCommands(DoCommandsJniEngine.strFunctionCallBack, "NULL");
							}
							else
							{
								this.view.renderer.onCallBackCommands(DoCommandsJniEngine.strFunctionCallBack, this.addUri(uri,picturePath));
							}
						}
						else
						{
							cursor.moveToFirst();
							int columnIndex = cursor.getColumnIndexOrThrow(filePathColumn[0]);
							String picturePath = cursor.getString(columnIndex);
							cursor.close();
							if (picturePath == null)
							{
								picturePath = uri.getPath();
								if (picturePath == null)
								{
									this.view.renderer.onCallBackCommands(DoCommandsJniEngine.strFunctionCallBack, "NULL");
								} else
								{
									this.view.renderer.onCallBackCommands(DoCommandsJniEngine.strFunctionCallBack, this.addUri(uri,picturePath));
								}
							} else
							{
								this.view.renderer.onCallBackCommands(DoCommandsJniEngine.strFunctionCallBack,this.addUri(uri,picturePath));
							}
						}
					}
				}
				catch (Exception e)
				{
					e.printStackTrace();
				}
			}
			else if(resultCode == Activity.RESULT_CANCELED)
			{
				this.refresh();
				this.view.renderer.onCallBackCommands(DoCommandsJniEngine.strFunctionCallBack,"CANCELED");
			}
			else
			{
				this.refresh();
			}
		}
		else
		{
			try
			{
				this.OnActivityResult(requestCode, resultCode,data);
			}
			catch (Exception e)
			{
				Log.d("debug_java","onActivityResult:"+ e.toString());
			}
		}
		super.onActivityResult(requestCode, resultCode, data);
	}

	@TargetApi(Build.VERSION_CODES.KITKAT)
	protected String getPathFromUri(final Uri uri)
	{
		final String scheme = uri.getScheme();
		if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) && DocumentsContract.isDocumentUri(this, uri))
		{
			if (this.isExternalStorageDocument(uri))// ExternalStorageProvider
			{
				final String docId = DocumentsContract.getDocumentId(uri);
				final String[]split = docId.split(":");
				final String type = split[0];

				if ("primary".equalsIgnoreCase(type))
					return Environment.getExternalStorageDirectory() + "/" + split[1];
			}
			else if (this.isDownloadsDocument(uri))// DownloadsProvider
			{
				final String id = DocumentsContract.getDocumentId(uri);
				final Uri contentUri = ContentUris.withAppendedId(Uri.parse("content://downloads/public_downloads"), Long.valueOf(id));
				return this.getDataColumn(contentUri, null, null);
			}
			else if (this.isMediaDocument(uri))// MediaProvider
			{
				final String docId = DocumentsContract.getDocumentId(uri);
				final String[]split = docId.split(":");
				final String type = split[0];
				Uri contentUri = null;
				switch (type)
				{
					case "image":
						contentUri = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
						break;
					case "video":
						contentUri = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
						break;
					case "audio":
						contentUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
						break;
				}
				final String selection = "_id=?";
				final String[]selectionArgs = new String[]{split[1]};
				return this.getDataColumn(contentUri, selection, selectionArgs);
			}
		}
		else if ("content".equalsIgnoreCase(scheme))// MediaStore (and general)
		{
			// Return the remote address
			if (isGooglePhotosUri(uri))
				return uri.getLastPathSegment();
			return this.getDataColumn(uri, null, null);
		}
		else if ("file".equalsIgnoreCase(scheme))// File
		{
			return uri.getPath();
		}
		return null;
	}

	/**
	 * Get the value of the data column for this Uri. This is useful for
	 * MediaStore Uris, and other file-based ContentProviders.
	 *
	 * @param uri The Uri to query.
	 * @param selection (Optional) Filter used in the query.
	 * @param selectionArgs (Optional) Selection arguments used in the query.
	 * @return The value of the _data column, which is typically a file path.
	 */
	protected String getDataColumn(Uri uri, String selection,String[]selectionArgs)
	{

		Cursor cursor = null;
		final String column = "_data";
		final String[]projection ={column};
		try
		{
			cursor = this.getContentResolver().query(uri, projection, selection, selectionArgs,null);
			if (cursor != null && cursor.moveToFirst())
			{
				final int index = cursor.getColumnIndexOrThrow(column);
				return cursor.getString(index);
			}
		}
		finally
		{
			if (cursor != null)
				cursor.close();
		}
		return null;
	}

	protected boolean isExternalStorageDocument(Uri uri)
	{
		return "com.android.externalstorage.documents".equals(uri.getAuthority());
	}

	protected boolean isDownloadsDocument(Uri uri)
	{
		return "com.android.providers.downloads.documents".equals(uri.getAuthority());
	}

	protected boolean isMediaDocument(Uri uri)
	{
		return "com.android.providers.media.documents".equals(uri.getAuthority());
	}

	protected boolean isGooglePhotosUri(Uri uri)
	{
		return "com.google.android.apps.photos.content".equals(uri.getAuthority());
	}


	private static class InputDeviceState
	{
		private final boolean isJoystick;
		private final InputDevice mDevice;
		private final int[] mAxes;
		private final float[] mFlats;
		private float lx,ly,rx,ry;
		private float Lx,Ly,Rx,Ry;

		private boolean onMoveJoystick(MotionEvent event)
		{
			for (int i = 0; i < mAxes.length; i++)
			{
				final int axis = mAxes[i];
				float value = event.getAxisValue(axis);
				if(value < this.mFlats[i] && value > (-this.mFlats[i]))
					value = 0;
				final String AxisName = MotionEvent.axisToString(axis);
				if(AxisName.compareTo("AXIS_X") == 0)
					lx = value;
				else if(AxisName.compareTo("AXIS_Y") == 0)
					ly = value;
				else if(AxisName.compareTo("AXIS_Z") == 0)
					rx = value;
				else if(AxisName.compareTo("AXIS_RZ") == 0)
					ry = value;
			}
			final boolean anyChange = lx != Lx || ly != Ly || Rx != rx || Ry != ry;
			if(anyChange)
			{
				Lx = lx;
				Ly = ly;
				Rx = rx;
				Ry = ry;
				InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
				instance.view.renderer.onMoveJoystick(1,lx,ly,rx,ry);
				return true;
			}
			return false;
		}

		private boolean isJoystickGameController(InputDevice dev)
		{
			if(dev != null)
			{
				final int sources = dev.getSources();
				// Verify that the device has gamepad buttons, control sticks, or both.
				return ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD)
						|| ((sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK);
			}
			return false;
		}

		@SuppressLint("NewApi")
		private InputDeviceState(InputDevice device)
		{
			this.mDevice = device;
			int numAxes = 0;
			final int maxButtons = 32;
			final List<MotionRange> ranges = device.getMotionRanges();
			for (MotionRange range : ranges)
			{
				if ((range.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0)
				{
					numAxes += 1;
				}
			}
			if(ranges.size()> 0 )
			{
				this.mAxes = new int[numAxes];
				this.mFlats = new float[numAxes];
				for (int i=0, j =0; i< ranges.size(); i++)
				{
					MotionRange range = ranges.get(i);
					if ((range.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0)
					{
						this.mAxes[j] = range.getAxis();
						this.mFlats[j] = range.getFlat();
						if(this.mFlats[j] < 0)
							this.mFlats[j] *= -1;
						j++;
					}
				}
			}
			else
			{
				this.mAxes = null;
				this.mFlats= null;
			}
			this.isJoystick = isJoystickGameController(device);
			if(numAxes > 0 && this.isJoystick)
			{
				final int player =  1;
				final String deviceName = this.mDevice.getName();
				final String extra = this.mDevice.getDescriptor();
				InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
				mainActivity.view.renderer.onInfoDeviceJoystick(player,maxButtons,deviceName,extra);
			}
		}
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
		super.onWindowFocusChanged(hasFocus);
		if (hasFocus) {
			hideSystemUI();
		}
	}

	@TargetApi(Build.VERSION_CODES.KITKAT)
	private void hideSystemUI() {
		// Enables regular immersive mode.
		// For "lean back" mode, remove SYSTEM_UI_FLAG_IMMERSIVE.
		// Or for "sticky immersive," replace it with SYSTEM_UI_FLAG_IMMERSIVE_STICKY
		View decorView = getWindow().getDecorView();
		decorView.setSystemUiVisibility(
				View.SYSTEM_UI_FLAG_IMMERSIVE
						// Set the content to appear under the system bars so that the
						// content doesn't resize when the system bars hide and show.
						| View.SYSTEM_UI_FLAG_LAYOUT_STABLE
						| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
						| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
						// Hide the nav bar and status bar
						| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
						| View.SYSTEM_UI_FLAG_FULLSCREEN);
		InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
		ActionBar actionBar = this.getSupportActionBar();
		if (actionBar != null)
		{
			actionBar.hide();
		}
	}

	// Shows the system bars by removing all the flags
	// except for the ones that make the content appear under the system bars.
	@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
	private void showSystemUI() {
		View decorView = getWindow().getDecorView();
		decorView.setSystemUiVisibility(
				View.SYSTEM_UI_FLAG_LAYOUT_STABLE
						| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
						| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
	}

	// Added the uri in the list and return the long name to use in the callback function lua
	private String addUri(Uri uri, String uriPath)
	{
		InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
		if (!instance.lsUri.containsKey(uriPath))
		{
			instance.lsUri.put(uriPath, uri);
		}
		int index = uriPath.lastIndexOf("/");
		if(index > 0)
		{
			String base_name = uriPath.substring(index+1);
			if(!instance.lsUri.containsKey(base_name))
				instance.lsUri.put(base_name, uri);
		}
		return uriPath;
	}

	protected void share(boolean ptBr)
    {
        int applicationNameId = this.getApplicationInfo().labelRes;
        final String appPackageName = this.getPackageName();
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, this.getString(applicationNameId));
        String text;
        if(ptBr)
            text = "Da uma olhada neste game\nMuito legal!\n ";
        else
            text = "Take a look at this Game\nCool!\n";
        String link = "https://play.google.com/store/apps/details?id=" + appPackageName;
        i.putExtra(Intent.EXTRA_TEXT, text + link);
        startActivity(Intent.createChooser(i, "Share link:"));
    }


	/**
	 * Checks if the app has permission to read/write to device storage
	 *
	 * If the app does not has permission then the user will be prompted to grant permissions
	 *
	 */
	@TargetApi(Build.VERSION_CODES.M)
	protected boolean hasStoragePermissions()
	{
		List<String> permissionsNeeded = new ArrayList<>();

		final List<String> permissionsList = new ArrayList<>();
		if (!addPermission(permissionsList, permission.READ_EXTERNAL_STORAGE))
			permissionsNeeded.add("Read External Storage");
		if (!addPermission(permissionsList, permission.WRITE_EXTERNAL_STORAGE))
			permissionsNeeded.add("Write External Storage");

		if (permissionsList.size() > 0 && permissionsNeeded.size() > 0)
		{
			requestPermissions(permissionsList.toArray(new String[permissionsList.size()]),REQUEST_CODE_ASK_READ_WRITE_SD_CARD);
			return false;
		}
		else
		{
			return true;
		}
	}

	@TargetApi(Build.VERSION_CODES.M)//23
	protected boolean hasVibrationPermissions()
	{
		final List<String> permissionsList = new ArrayList<>();
		if (!addPermission(permissionsList, permission.VIBRATE))
		{
			requestPermissions(permissionsList.toArray(new String[permissionsList.size()]),REQUEST_CODE_ASK_FOR_VIBRATION);
			return false;
		}
		else
		{
			return true;
		}
	}

	@TargetApi(Build.VERSION_CODES.M)//23
	private boolean addPermission(List<String> permissionsList, String permission)
	{
		if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED)
		{
			permissionsList.add(permission);
			return false;
		}
		return true;
	}

	@Override
	@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
	{
		switch (requestCode)
		{
			case REQUEST_CODE_ASK_READ_WRITE_SD_CARD:
			case REQUEST_CODE_ASK_FOR_VIBRATION:
			{
				Map<String, Integer> perms = new HashMap<String, Integer>();
				
				for (int i = 0; i < permissions.length; i++)
				{
					perms.put(permissions[i], grantResults[i]);
				}

				for (String key : perms.keySet())
				{
					Integer granted_perm = perms.get(key);
					if(granted_perm != null)
					{
						// Check for ACCESS
						if (granted_perm != PackageManager.PERMISSION_GRANTED)
						{
							// Permission Denied
							Toast.makeText(this, "Permission:" + key + " is Denied", Toast.LENGTH_SHORT).show();
						}
					}
				}
			}
			break;
			default:
				super.onRequestPermissionsResult(requestCode, permissions, grantResults);
		}
	}
}
