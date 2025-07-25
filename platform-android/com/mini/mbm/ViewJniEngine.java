package com.mini.mbm;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.Log;

import java.util.Vector;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

@SuppressLint("ViewConstructor")
public class ViewJniEngine extends GLSurfaceView
{
	private static String TAG = "debug_java";
	public Renderer renderer;

	public ViewJniEngine(Context context,Activity activity,final String versionAPP,final int simultaneousStreams,int expectedWidth,int expectedHeight)
	{
		super(context);
		final String absPath = context.getFilesDir().getAbsolutePath() + "/";
		final String apkPath = context.getPackageResourcePath() + "/";
		/***IMPORTANT***/
		InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance(activity,absPath,apkPath,versionAPP );
		mainActivity.activity = activity;
		AudioManagerJniEngine.getInstance(simultaneousStreams);
		mainActivity.view = this;
		this.init(false,1, 0,absPath,apkPath,expectedWidth,expectedHeight);
	}

	private void init(boolean translucent, int depth, int stencil,String absPath,String apkPath,int expectedWidth,int expectedHeight)
	{

		/*
		 * By default, GLSurfaceView() creates a RGB_565 opaque surface. If we
		 * want a translucent one, we should change the surface's format here,
		 * using PixelFormat.TRANSLUCENT for GL Surfaces is interpreted as any
		 * 32-bit surface with alpha by SurfaceFlinger.
		 */
		if (translucent)
		{
			this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
		}

		/*
		 * Setup the context factory for 2.0 rendering. See ContextFactory class
		 * definition below
		 */
		setEGLContextFactory(new ContextFactory());

		/*
		 * We need to choose an EGLConfig that matches the format of our surface
		 * exactly. This is going to be done in our custom config chooser. See
		 * ConfigChooser class definition below.
		 */
		/*
		 
    First, you need to edit your emulator image, go down to the hardware section, and add �GPU Emulation� and set it to true.
    Second, there�s a bug with the emulator such that this line: �final boolean supportsEs2 = configurationInfo.reqGlEsVersion >= 0�20000;� does not work. It will always return false. You can add �|| Build.FINGERPRINT.startsWith(�generic�)� or simply comment out these checks and assume that OpenGL ES 2 is supported, when running on the emulator.
    Finally, if it crashes with �no config found�, try adding this line before the call to �setRenderer(�)�: �glSurfaceView.setEGLConfigChooser(8 , 8, 8, 8, 16, 0);�

		 */
		//Emulator
		//setEGLConfigChooser(8 , 8, 8, 8, 16, 0);
		setEGLConfigChooser(translucent ? new ConfigChooser(8, 8, 8, 8, depth,
				stencil) : new ConfigChooser(5, 6, 5, 0, depth, stencil));

		/* Set the renderer responsible for frame rendering */
		renderer = new Renderer(absPath,apkPath,expectedWidth,expectedHeight);
		setRenderer(renderer);
		//this.setRenderMode(RENDERMODE_CONTINUOUSLY);
	}

	private static class ContextFactory implements	GLSurfaceView.EGLContextFactory
	{
		private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

		public EGLContext createContext(EGL10 egl, EGLDisplay display,
				EGLConfig eglConfig)
		{
			Log.w(TAG, "creating OpenGL ES 2.0 context");
			checkEglError("Before eglCreateContext", egl);
			int[] attrib_list =
			{ EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };
			EGLContext context = egl.eglCreateContext(display, eglConfig,
					EGL10.EGL_NO_CONTEXT, attrib_list);
			checkEglError("After eglCreateContext", egl);
			return context;
		}

		public void destroyContext(EGL10 egl, EGLDisplay display,EGLContext context)
		{
			egl.eglDestroyContext(display, context);
		}
	}

	private static void checkEglError(String prompt, EGL10 egl)
	{
		int error;
		while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS)
		{
			Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
		}
	}

	private static class ConfigChooser implements	GLSurfaceView.EGLConfigChooser
	{

		public ConfigChooser(int r, int g, int b, int a, int depth, int stencil)
		{
			mRedSize = r;
			mGreenSize = g;
			mBlueSize = b;
			mAlphaSize = a;
			mDepthSize = depth;
			mStencilSize = stencil;
		}

		/*
		 * This EGL config specification is used to specify 2.0 rendering. We
		 * use a minimum size of 4 bits for red/green/blue, but will perform
		 * actual matching in chooseConfig() below.
		 */
		private static int EGL_OPENGL_ES2_BIT = 4;
		private static int[] s_configAttribs2 =
		{ EGL10.EGL_RED_SIZE, 4, EGL10.EGL_GREEN_SIZE, 4, EGL10.EGL_BLUE_SIZE,
				4, EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
				EGL10.EGL_NONE };

		public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display)
		{

			/*
			 * Get the number of minimally matching EGL configurations
			 */
			int[] num_config = new int[1];
			egl.eglChooseConfig(display, s_configAttribs2, null, 0, num_config);

			int numConfigs = num_config[0];

			if (numConfigs <= 0)
			{
				throw new IllegalArgumentException(
						"No configs match configSpec");
			}

			/*
			 * Allocate then read the array of minimally matching EGL configs
			 */
			EGLConfig[] configs = new EGLConfig[numConfigs];
			egl.eglChooseConfig(display, s_configAttribs2, configs, numConfigs,
					num_config);

			//printConfigs(egl, display, configs);
			/*
			 * Now return the "best" one
			 */
			return chooseConfig(egl, display, configs);
		}

		public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display, EGLConfig[] configs)
		{
			for (EGLConfig config : configs)
			{
				int d = findConfigAttrib(egl, display, config,
						EGL10.EGL_DEPTH_SIZE, 0);
				int s = findConfigAttrib(egl, display, config,
						EGL10.EGL_STENCIL_SIZE, 0);

				// We need at least mDepthSize and mStencilSize bits
				if (d < mDepthSize || s < mStencilSize)
					continue;

				// We want an *exact* match for red/green/blue/alpha
				int r = findConfigAttrib(egl, display, config,
						EGL10.EGL_RED_SIZE, 0);
				int g = findConfigAttrib(egl, display, config,
						EGL10.EGL_GREEN_SIZE, 0);
				int b = findConfigAttrib(egl, display, config,
						EGL10.EGL_BLUE_SIZE, 0);
				int a = findConfigAttrib(egl, display, config,
						EGL10.EGL_ALPHA_SIZE, 0);

				if (r == mRedSize && g == mGreenSize && b == mBlueSize
						&& a == mAlphaSize)
					return config;
			}
			return null;
		}

		private int findConfigAttrib(EGL10 egl, EGLDisplay display,
				EGLConfig config, int attribute, int defaultValue)
		{

			if (egl.eglGetConfigAttrib(display, config, attribute, mValue))
			{
				return mValue[0];
			}
			return defaultValue;
		}

		@SuppressWarnings("unused")
		void printConfigs(EGL10 egl, EGLDisplay display,
				EGLConfig[] configs)
		{
			int numConfigs = configs.length;
			Log.w(TAG, String.format("%d configurations", numConfigs));
			for (int i = 0; i < numConfigs; i++)
			{
				Log.w(TAG, String.format("Configuration %d:\n", i));
				printConfig(egl, display, configs[i]);
			}
		}

		private void printConfig(EGL10 egl, EGLDisplay display, EGLConfig config)
		{
			int[] attributes =
			{ EGL10.EGL_BUFFER_SIZE, EGL10.EGL_ALPHA_SIZE, EGL10.EGL_BLUE_SIZE,
					EGL10.EGL_GREEN_SIZE,
					EGL10.EGL_RED_SIZE,
					EGL10.EGL_DEPTH_SIZE,
					EGL10.EGL_STENCIL_SIZE,
					EGL10.EGL_CONFIG_CAVEAT,
					EGL10.EGL_CONFIG_ID,
					EGL10.EGL_LEVEL,
					EGL10.EGL_MAX_PBUFFER_HEIGHT,
					EGL10.EGL_MAX_PBUFFER_PIXELS,
					EGL10.EGL_MAX_PBUFFER_WIDTH,
					EGL10.EGL_NATIVE_RENDERABLE,
					EGL10.EGL_NATIVE_VISUAL_ID,
					EGL10.EGL_NATIVE_VISUAL_TYPE,
					0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
					EGL10.EGL_SAMPLES,
					EGL10.EGL_SAMPLE_BUFFERS,
					EGL10.EGL_SURFACE_TYPE,
					EGL10.EGL_TRANSPARENT_TYPE,
					EGL10.EGL_TRANSPARENT_RED_VALUE,
					EGL10.EGL_TRANSPARENT_GREEN_VALUE,
					EGL10.EGL_TRANSPARENT_BLUE_VALUE,
					0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
					0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
					0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
					0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
					EGL10.EGL_LUMINANCE_SIZE, EGL10.EGL_ALPHA_MASK_SIZE,
					EGL10.EGL_COLOR_BUFFER_TYPE, EGL10.EGL_RENDERABLE_TYPE,
					0x3042 // EGL10.EGL_CONFORMANT
			};
			String[] names =
			{ "EGL_BUFFER_SIZE", "EGL_ALPHA_SIZE", "EGL_BLUE_SIZE",
					"EGL_GREEN_SIZE", "EGL_RED_SIZE", "EGL_DEPTH_SIZE",
					"EGL_STENCIL_SIZE", "EGL_CONFIG_CAVEAT", "EGL_CONFIG_ID",
					"EGL_LEVEL", "EGL_MAX_PBUFFER_HEIGHT",
					"EGL_MAX_PBUFFER_PIXELS", "EGL_MAX_PBUFFER_WIDTH",
					"EGL_NATIVE_RENDERABLE", "EGL_NATIVE_VISUAL_ID",
					"EGL_NATIVE_VISUAL_TYPE", "EGL_PRESERVED_RESOURCES",
					"EGL_SAMPLES", "EGL_SAMPLE_BUFFERS", "EGL_SURFACE_TYPE",
					"EGL_TRANSPARENT_TYPE", "EGL_TRANSPARENT_RED_VALUE",
					"EGL_TRANSPARENT_GREEN_VALUE",
					"EGL_TRANSPARENT_BLUE_VALUE", "EGL_BIND_TO_TEXTURE_RGB",
					"EGL_BIND_TO_TEXTURE_RGBA", "EGL_MIN_SWAP_INTERVAL",
					"EGL_MAX_SWAP_INTERVAL", "EGL_LUMINANCE_SIZE",
					"EGL_ALPHA_MASK_SIZE", "EGL_ColorJniEngine_BUFFER_TYPE",
					"EGL_RENDERABLE_TYPE", "EGL_CONFORMANT" };
			int[] value = new int[1];
			for (int i = 0; i < attributes.length; i++)
			{
				int attribute = attributes[i];
				String name = names[i];
				if (egl.eglGetConfigAttrib(display, config, attribute, value))
				{
					Log.w(TAG, String.format("  %s: %d\n", name, value[0]));
				}
				else
				{

					while (egl.eglGetError() != EGL10.EGL_SUCCESS)
					{
						Log.w(TAG, String.format("  %s: failed\n", name));
					}
				}
			}
		}

		// Subclasses can adjust these values:
		protected int mRedSize;
		protected int mGreenSize;
		protected int mBlueSize;
		protected int mAlphaSize;
		protected int mDepthSize;
		protected int mStencilSize;
		private int[] mValue = new int[1];
	}

	public static class Renderer implements GLSurfaceView.Renderer
	{
		private Vector<EventJniEngine> lsEvent;
		private float lastEventTouchDown_x;
		private float lastEventTouchDown_y;
		private float lastEventTouchMove_x;
		private float lastEventTouchMove_y;
		private int   expectedWidth;
		private int   expectedHeight;
		private boolean ready_for_events;

		public Renderer(final String absPath,final String apkPath,int expectedWidth,int expectedHeight)
		{
			currentAbsPathApp = absPath;
			currentApkPathApp = apkPath;
			this.lsEvent = new Vector<>();
			this.lastEventTouchDown_x = 0;
			this.lastEventTouchDown_y = 0;
			this.lastEventTouchMove_x = 0;
			this.lastEventTouchMove_y = 0;
			this.expectedWidth = expectedWidth;
			this.expectedHeight = expectedHeight;
			this.ready_for_events = false;
		}
		
		public void onSurfaceChanged(GL10 gl, int width, int height)
		{
			try
			{
				InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
				mainActivity.gl10 = gl;
				if(MiniMbmEngine.needRestart)
				{
					
					if(MiniMbmEngine.onRestoreDevice(mainActivity.widthScreen,mainActivity.heightScreen))
						MiniMbmEngine.needRestart = false;
				}
				else
				{
					mainActivity.widthScreen = width;
					mainActivity.heightScreen = height;
					MiniMbmEngine.init(width, height,currentAbsPathApp,currentApkPathApp,expectedWidth,expectedHeight);
				}
			}
			catch (Exception e) 
			{
				System.out.println(e.toString());
			}
		}
		
		public void onDrawFrame(GL10 gl)
		{
			try
			{
				InstanceActivityEngine.getInstance().gl10 = gl;
				if(MiniMbmEngine.needRestart)
				{
					if(MiniMbmEngine.onRestoreDevice(0,0))
						MiniMbmEngine.needRestart = false;
				}
				else
				{
					if(this.ready_for_events)
					{
						synchronized(this.lsEvent)
						{
							for(int i=0;i< this.lsEvent.size(); i++)
							{
								EventJniEngine event = this.lsEvent.get(i);
								switch(event.eventType)
								{
									case onKeyDown:
									{
										MiniMbmEngine.onKeyDown(event.key);
									}
									break;
									case onKeyDownJoystick:
									{
										MiniMbmEngine.onKeyDownJoystick((int)event.x,event.key);
									}
									break;
									case onKeyUp:
									{
										MiniMbmEngine.onKeyUp(event.key);
									}
									break;
									case onKeyUpJoystick:
									{
										MiniMbmEngine.onKeyUpJoystick((int) event.x, event.key);
									}
									break;
									case onInfoDeviceJoystick:
									{
										MiniMbmEngine.onInfoDeviceJoystick((int)event.x,event.key,event.param,event.param2);
									}
									break;
									case onMoveJoystick:
									{
										final float rx = Float.parseFloat(event.param);
										final float ry = Float.parseFloat(event.param2);
										MiniMbmEngine.onMoveJoystick(event.key,event.x,event.y,rx,ry);
									}
									break;
									case onTouchDown:
									{
										MiniMbmEngine.onTouchDown(event.key,event.x,event.y);
									}
									break;
									case onTouchMove:
									{
										MiniMbmEngine.onTouchMove(event.key,event.x,event.y);
									}
									break;
									case onTouchZoom:
									{
										MiniMbmEngine.onTouchZoom(event.x);
									}
									break;
									case onTouchUp:
									{
										MiniMbmEngine.onTouchUp(event.key,event.x,event.y);
									}
									break;
									case onStreamStopped:
									{
										MiniMbmEngine.streamStopped(event.key);
									}
									break;
									case onCallBackCommands:
									{
										MiniMbmEngine.onCallBackCommands(event.param,event.param2);
									}
								}
							}
							this.lsEvent.clear();
						}
					}
					if(InstanceActivityEngine.run)
					{
						MiniMbmEngine.loop();
						this.ready_for_events = true;
					}
					else
					{
						InstanceActivityEngine main =  InstanceActivityEngine.getInstance();
						if(!main.activity.isFinishing())
							main.activity.finish();
						this.ready_for_events = false;
					}
				}
				
			}
			catch (Exception e) 
			{
				System.out.println(e.toString());
			}
		}

		public void onSurfaceCreated(GL10 gl, EGLConfig config)
		{
			
		}
		public void callLuaFunction(String function,String Param)
        {
            EventJniEngine ev = new EventJniEngine(0,0, 0, EventJniEngineName.onKeyDown,function,Param);
            synchronized(this.lsEvent)
            {
				this.lsEvent.add(ev);
            }
        }
		
		public void onKeyDown(int key)
		{
			EventJniEngine ev = new EventJniEngine(0,0, key, EventJniEngineName.onKeyDown,null,null);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}
		
		public void onKeyUp(int key)
		{
			EventJniEngine ev = new EventJniEngine(0,0, key, EventJniEngineName.onKeyUp,null,null);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}

		public void onKeyDownJoystick(int player,int key)
		{
			EventJniEngine ev = new EventJniEngine((float)player,0, key, EventJniEngineName.onKeyDownJoystick,null,null);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}

		public void onKeyUpJoystick(int player,int key)
		{
			EventJniEngine ev = new EventJniEngine((float)player,0, key, EventJniEngineName.onKeyUpJoystick,null,null);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}

		public void onInfoDeviceJoystick(int player, int maxNumberButton, String deviceName,String extraInfo)
		{
			EventJniEngine ev = new EventJniEngine((float)player,0, maxNumberButton, EventJniEngineName.onInfoDeviceJoystick,deviceName,extraInfo);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}

		public void onMoveJoystick(int player, float lx, float ly, float rx, float ry)
		{
			final String str_rx = Float.toString(rx);
			final String str_ry = Float.toString(ry);
			EventJniEngine ev = new EventJniEngine(lx,ly, player, EventJniEngineName.onMoveJoystick,str_rx,str_ry);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}

		public void onTouchDown(int key ,float x,float y)
		{
			EventJniEngine ev = new EventJniEngine(x, y, key, EventJniEngineName.onTouchDown,null,null);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
			this.lastEventTouchDown_x = x;
			this.lastEventTouchDown_y = y;
		}
		public void onTouchUp(int key ,float x,float y)
		{
			EventJniEngine ev = new EventJniEngine(x, y, key, EventJniEngineName.onTouchUp,null,null);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);

			}
		}
		public void onTouchMove(int key ,float x,float y)
		{
			if(( this.lastEventTouchDown_x != x || this.lastEventTouchDown_y != y) &&
				(this.lastEventTouchMove_x != x || this.lastEventTouchMove_y != y))
			{
				EventJniEngine ev = new EventJniEngine(x, y, key, EventJniEngineName.onTouchMove,null,null);
				synchronized(this.lsEvent)
				{
					this.lsEvent.add(ev);
				}
				this.lastEventTouchMove_x = x;
				this.lastEventTouchMove_y = y;
			}
		}
		public void onTouchZoom(float zoom) 
		{
			EventJniEngine ev = new EventJniEngine(zoom, zoom,0 , EventJniEngineName.onTouchZoom,null,null);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}
		
		public void onCallBackCommands(final String param,final String param2) 
		{
			EventJniEngine ev = new EventJniEngine(0,0,0 , EventJniEngineName.onCallBackCommands,param,param2);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}
		
		public void OnStreamStopped(int indexJni)
		{
			EventJniEngine ev = new EventJniEngine(0, 0,indexJni, EventJniEngineName.onStreamStopped,null,null);
			synchronized(this.lsEvent)
			{
				this.lsEvent.add(ev);
			}
		}
		private String currentApkPathApp;
		private String currentAbsPathApp;
		
		
	}
}
