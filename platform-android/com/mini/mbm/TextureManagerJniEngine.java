package com.mini.mbm;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLU;
import android.opengl.GLUtils;
import android.util.Log;

import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.Vector;

import javax.microedition.khronos.opengles.GL10;




public class TextureManagerJniEngine
{

	private static TextureManagerJniEngine instanceTextureManager = null;
	private Vector<TEXTURE>			lsTextures;
	private IntBuffer 				maxTextureSize;

	public TextureManagerJniEngine(GL10 gl)
	{
		lsTextures 		= 	new Vector<>();
		maxTextureSize 	= IntBuffer.allocate(1) ;
		gl.glGetIntegerv(GL10.GL_MAX_TEXTURE_SIZE, maxTextureSize);
	}

	public static TextureManagerJniEngine getInstance(GL10 gl)
	{
		if(instanceTextureManager==null)
			instanceTextureManager = new TextureManagerJniEngine(gl);
		return instanceTextureManager;
	}

	public final int getMaxTextureSize()
	{
		return this.maxTextureSize.get(0);
	}

	public static void release(GL10 gl)
	{
		if(instanceTextureManager!=null)
		{
			int s = instanceTextureManager.lsTextures.size();
			for(int x = 0; x< s; x++)
			{
				TEXTURE  ptr = instanceTextureManager.lsTextures.get(x);
				if(ptr != null)
					ptr.release(gl);
			}
			instanceTextureManager.lsTextures.clear();
		}
		instanceTextureManager = null;
	}

	public TEXTURE load(final GL10 gl,final Resources resources, final int idImageR)
	{
		for(int i = 0, s = lsTextures.size(); i< s; i++)
		{
			TEXTURE texture = lsTextures.get(i);
	
			if(!texture.hasColorKeying && texture.idImageResource == idImageR)
				return texture;
		}
		TEXTURE texture = new TEXTURE();
		texture.load(gl,resources,idImageR);
		lsTextures.add(texture);
		return texture;
	}

	public TEXTURE load(final GL10 gl,final String fileName)
	{
		for(int i = 0, s = lsTextures.size(); i< s; i++)
		{
			TEXTURE texture = lsTextures.get(i);
				
			if(!texture.hasColorKeying && texture.fileNameTexture != null && texture.fileNameTexture.compareTo(fileName) == 0)
				return texture;
		}
		TEXTURE texture = new TEXTURE();
		try
		{
			Bitmap bitmap = FileJniEngine.getThumbnail(fileName,false);
			final int maxSize = maxTextureSize.get(0);
			if(bitmap == null || bitmap.getWidth() > maxSize || bitmap.getHeight() > maxSize )
			{
				bitmap = FileJniEngine.getThumbnail(fileName,true);
				if(bitmap == null)
				{
					return null;
				}
			}
			bitmap = FileJniEngine.rotateBitmap(bitmap);
		    texture.load(gl,bitmap,fileName);
		}
		catch (Exception e) 
		{
			if(InstanceActivityEngine.debugMessage)
				Log.d("debug_java","Error on load texture: "+ fileName);
			return null;
		}
		lsTextures.add(texture);
		return texture;
	}

	public TEXTURE load(final GL10 gl,final Resources resources,final ColorJniEngine colorKeying,final int idImageR)
	{
		for(int i = 0, s = lsTextures.size(); i< s; i++)
		{
			TEXTURE texture = lsTextures.get(i);
			if(texture.hasColorKeying && texture.idImageResource == idImageR)
				return texture;
		}
		TEXTURE texture = new TEXTURE();
		texture.load(gl,resources,colorKeying,idImageR);
		lsTextures.add(texture);
		return texture;
	}

	public TEXTURE load(final GL10 gl,final String fileName,final ColorJniEngine colorKeying)
	{
		for(int i = 0, s = lsTextures.size(); i< s; i++)
		{
			TEXTURE texture = lsTextures.get(i);
	
			if(texture.hasColorKeying && texture.fileNameTexture != null && texture.fileNameTexture.compareTo(fileName) == 0)
				return texture;
		}
		TEXTURE texture = new TEXTURE();
		try
		{
			InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
			InputStream in =  mainActivity.getAssets().open(fileName);
		    Bitmap bitmap = BitmapFactory.decodeStream(in);
		    texture.load(gl,bitmap,colorKeying,fileName);
		}
		catch (Exception e) 
		{
			if(InstanceActivityEngine.debugMessage)
				Log.d("debug_java","Error on load textura: "+ fileName);
			return null;
		}
		lsTextures.add(texture);
		return texture;
	}

	public TEXTURE get(final int idImageR)
	{
		for(int i = 0, s = lsTextures.size(); i< s; i++)
		{
			TEXTURE texture = lsTextures.get(i);
			if(texture.getIdImageResource() == idImageR)
				return texture;
		}
		return null;
	}
	
	public class TEXTURE
	{

		public ColorJniEngine			colorMask;
		private int[]			idTexture;
		public boolean			hasColorKeying;
		private	int				idImageResource;
		private String			fileNameTexture;
		private int				width;
		private int				height;

		public TEXTURE()
		{
			colorMask 		= 	new ColorJniEngine(1.0f,1.0f,1.0f,1.0f);
			idTexture 		= 	null;
			fileNameTexture = 	null;
			idImageResource	=	-1;
		}

		public int getWidth()
		{
			return this.width;
		}

		public int getHeight()
		{
			return this.height;
		}

		public final int [] getIdTexture()
		{
			return idTexture;
		}

		public final int getIdImageResource()
		{
			return idImageResource;
		}

		private void load(final GL10 gl,final Resources resources, final int idImageR)
		{
			if(idTexture != null)
				return;
			idImageResource = idImageR;
			Bitmap mBitmap = BitmapFactory.decodeResource(resources,idImageResource);
			if(mBitmap == null)
			{
				if(InstanceActivityEngine.debugMessage)
					Log.d("debug_java", "Error on load texture: " +idImageR +" Bitmap null!");
				return;
			}
			idTexture = new int[1];
			idTexture[0]	=	-1;
			gl.glEnable(GL10.GL_TEXTURE_2D);
			// Generate one texture pointer...
			gl.glGenTextures(1, idTexture, 0);
			if(idTexture[0] == -1)
			{
				gl.glGenTextures(-1, idTexture, 0);
				int err = gl.glGetError();
				if(err != GL10.GL_NO_ERROR)
				{
					if(InstanceActivityEngine.debugMessage)
						Log.d("mbm","Error on load texture! Err GL: " + GLU.gluErrorString(err));
					idTexture[0] =  lsTextures.size()+1;
				}
			}
			// ...and bind it to our array
			gl.glBindTexture(GL10.GL_TEXTURE_2D, idTexture[0]);

			// Create Nearest Filtered Texture
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER,
					GL10.GL_LINEAR);
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER,
					GL10.GL_LINEAR);

			// Different possible texture parameters, e.g. GL10.GL_CLAMP_TO_EDGE
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S,
					GL10.GL_CLAMP_TO_EDGE);
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T,
					GL10.GL_REPEAT);
			
			// Use the Android GLUtils to specify a two-dimensional texture image
			// from our bitmap
			GLUtils.texImage2D(GL10.GL_TEXTURE_2D, 0, mBitmap, 0);
			hasColorKeying = false;
			int err = gl.glGetError();
			if(err == GL10.GL_NO_ERROR)
			{
				if(InstanceActivityEngine.debugMessage)
				{
					if(this.fileNameTexture == null)
						Log.d("debug_java", "Texture: '" + idTexture[0]+ "' created.");
					else
						Log.d("debug_java", "Texture: '" + this.fileNameTexture+ "' created.");
				}
			}
			this.width  = mBitmap.getWidth(); 
			this.height = mBitmap.getHeight();
			this.hasColorKeying = mBitmap.hasAlpha();
			mBitmap.recycle();
		}

		private void load(final GL10 gl,final Bitmap mBitmap,final String fileName)
		{
			if(idTexture != null)
				return;
			fileNameTexture = fileName;
			if(mBitmap == null)
			{
				if(InstanceActivityEngine.debugMessage)
					Log.d("debug_java", "Error on load texture: " +fileName +" Bitmap null!");
				return;
			}
			idTexture = new int[1];
			idTexture[0]	=	-1;
			gl.glEnable(GL10.GL_TEXTURE_2D);
			// Generate one texture pointer...
			gl.glGenTextures(1, idTexture, 0);
			if(idTexture[0] == -1)
			{
				
				gl.glGenTextures(-1, idTexture, 0);
				int err = gl.glGetError();
				if(err != GL10.GL_NO_ERROR)
				{
					idTexture[0] =  lsTextures.size()+1;
				}
				else
				{
					if(InstanceActivityEngine.debugMessage)
						Log.d("debug_java","Error on load texture! Err GL: " + GLU.gluErrorString(err));
				}
			}
			// ...and bind it to our array
			gl.glBindTexture(GL10.GL_TEXTURE_2D, idTexture[0]);

			// Create Nearest Filtered Texture
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER,
					GL10.GL_LINEAR);
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER,
					GL10.GL_LINEAR);

			// Different possible texture parameters, e.g. GL10.GL_CLAMP_TO_EDGE
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S,
					GL10.GL_CLAMP_TO_EDGE);
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T,
					GL10.GL_REPEAT);
			
			// Use the Android GLUtils to specify a two-dimensional texture image
			// from our bitmap
			GLUtils.texImage2D(GL10.GL_TEXTURE_2D, 0, mBitmap, 0);
			hasColorKeying = false;
			int err = gl.glGetError();
			if(err == GL10.GL_NO_ERROR)
			{
				if(InstanceActivityEngine.debugMessage)
				{
					if(this.fileNameTexture == null)
						Log.d("debug_java", "Texture: '" + idTexture[0]+ "' created.");
					else
						Log.d("debug_java", "Texture: '" + this.fileNameTexture+ "' created.");
				}
			}
			this.width  = mBitmap.getWidth(); 
			this.height = mBitmap.getHeight();
			this.hasColorKeying = mBitmap.hasAlpha(); 
			mBitmap.recycle();
		}

		private void load(final GL10 gl,final Resources resources,final ColorJniEngine colorKeying, final int idImageR)//Carrega a textura informando o resource, a cor de recorte colorkeying e o id da imagem
		{
			if(idTexture != null)
				return;
			idImageResource = idImageR;
			Bitmap mBitmap = BitmapFactory.decodeResource(resources,idImageResource);
			if(mBitmap == null)
			{
				if(InstanceActivityEngine.debugMessage)
					Log.d("debug_java", "Error on load texture: " +idImageR + " Bitmap null!");
				return;
			}
			idTexture = new int[1];
			idTexture[0]	=	-1;
			gl.glEnable(GL10.GL_TEXTURE_2D);
			// Generate one texture pointer...
			gl.glGenTextures(1, idTexture, 0);
			int err = gl.glGetError();
			if(err != GL10.GL_NO_ERROR)
			{
				if(InstanceActivityEngine.debugMessage)
					Log.d("debug_java","Error on create texture! Err GL: " + GLU.gluErrorString(err));
				err = gl.glGetError();
				if(err != GL10.GL_NO_ERROR)
				{
					idTexture[0] =  lsTextures.size()+1;
				}
			}
						// ...and bind it to our array
			gl.glBindTexture(GL10.GL_TEXTURE_2D, idTexture[0]);

			// Create Nearest Filtered Texture
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER,
					GL10.GL_LINEAR);
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER,
					GL10.GL_LINEAR);

			// Different possible texture parameters, e.g. GL10.GL_CLAMP_TO_EDGE
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S,
					GL10.GL_CLAMP_TO_EDGE);
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T,
					GL10.GL_REPEAT);
			
			final int h = mBitmap.getHeight();
			final int w = mBitmap.getWidth();
			
			final int r = (int) (colorKeying.r * 255.0f);
			final int g = (int) (colorKeying.g * 255.0f);
			final int b = (int) (colorKeying.b * 255.0f);
			
			int image[] = new int [w*h];
			int i=0;
			for ( int y = 0; y < h; y++ )
			{
				for (int x = 0; x < w; x++)
				{
					final int pixel = mBitmap.getPixel(x, y);//0XAABBGGRR
					final int red = (pixel >> 16) & 0xFF;
					final int green = (pixel >> 8) & 0xFF;
					final int blue = (pixel) & 0xFF;
					if (red == r && green == g && blue == b)
						image[i] = ((0X00 << 24) | (blue << 16) | (green << 8) | red);//Opaco
					else
						image[i] = ((0XFF << 24) | (blue << 16) | (green << 8) | red);//Transparente
					i++;
				}
			}
			ByteBuffer bbuffer 	= ByteBuffer.allocateDirect(w * h * 4);
			bbuffer.order(ByteOrder.nativeOrder());
			IntBuffer imageBuffer = bbuffer.asIntBuffer();
			imageBuffer.put(image);
			imageBuffer.position(0);
				
			gl.glTexImage2D(GL10.GL_TEXTURE_2D, 0, GL10.GL_RGBA,w, h, 0,
					GL10.GL_RGBA, GL10.GL_UNSIGNED_BYTE, imageBuffer);
			hasColorKeying = true;
			if(err == GL10.GL_NO_ERROR)
			{
				if(InstanceActivityEngine.debugMessage)
				{
					if(this.fileNameTexture == null)
						Log.d("debug_java", "Texture colorKeying number: '" + idTexture[0]+ "' created.");
					else
						Log.d("debug_java", "Texture colorKeying: '" + this.fileNameTexture+ "' created.");
				}
			}
			imageBuffer.clear();
			this.width  = mBitmap.getWidth();
			this.height = mBitmap.getHeight();
			this.hasColorKeying = mBitmap.hasAlpha();
			mBitmap.recycle();
		}
		
		private void load(final GL10 gl,Bitmap mBitmap,final ColorJniEngine colorKeying,final String fileName)
		{
			if(idTexture != null)
				return;
			fileNameTexture = fileName;
			if(mBitmap == null)
			{
				if(InstanceActivityEngine.debugMessage)
					Log.d("debug_java", "Error on create: " +fileNameTexture + " Bitmap null!");
				return;
			}
			idTexture = new int[1];
			idTexture[0]	=	-1;
			gl.glEnable(GL10.GL_TEXTURE_2D);
			// Generate one texture pointer...
			gl.glGenTextures(1, idTexture, 0);
			int err = gl.glGetError();
			if(err != GL10.GL_NO_ERROR)
			{
				if(InstanceActivityEngine.debugMessage)
					Log.d("debug_java","Error on create texture! Err GL: " + GLU.gluErrorString(err));
				err = gl.glGetError();
				if(err != GL10.GL_NO_ERROR)
				{
					idTexture[0] =  lsTextures.size()+1;
				}
			}
						// ...and bind it to our array
			gl.glBindTexture(GL10.GL_TEXTURE_2D, idTexture[0]);

			// Create Nearest Filtered Texture
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER,
					GL10.GL_LINEAR);
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER,
					GL10.GL_LINEAR);

			// Different possible texture parameters, e.g. GL10.GL_CLAMP_TO_EDGE
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S,
					GL10.GL_CLAMP_TO_EDGE);
			gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T,
					GL10.GL_REPEAT);
			
			final int h = mBitmap.getHeight();
			final int w = mBitmap.getWidth();
			
			final int r = (int) (colorKeying.r * 255.0f);
			final int g = (int) (colorKeying.g * 255.0f);
			final int b = (int) (colorKeying.b * 255.0f);
			
			int image[] = new int [w*h];
			int i=0;
			for ( int y = 0; y < h; y++ )
			{
			    for ( int x = 0; x < w; x++ )
			    {
				    final int pixel = mBitmap.getPixel(x, y);//0XAABBGGRR
				    final int red   = (pixel >> 16) & 0xFF;
	                final int green = (pixel >> 8 ) & 0xFF;
	                final int blue  = (pixel)       & 0xFF;
	                if(red == r && green == g && blue == b)
	                	image[i] = ((0X00 << 24) | (blue<<16) | (green << 8) | red);//Opaco
	                else
	                	image[i] = ((0XFF << 24) | (blue<<16) | (green << 8) | red);//Transparente
	                i++;
			    }
			}
			 
			ByteBuffer bbuffer 	= ByteBuffer.allocateDirect(w * h * 4);
			bbuffer.order(ByteOrder.nativeOrder());
			IntBuffer imageBuffer = bbuffer.asIntBuffer();
			imageBuffer.put(image);
			imageBuffer.position(0);
				
			gl.glTexImage2D(GL10.GL_TEXTURE_2D, 0, GL10.GL_RGBA,w, h, 0,
					GL10.GL_RGBA, GL10.GL_UNSIGNED_BYTE, imageBuffer);
			hasColorKeying = true;
			if(err == GL10.GL_NO_ERROR)
			{
				if(InstanceActivityEngine.debugMessage)
				{
					if(this.fileNameTexture==null)
						Log.d("debug_java", "Texture colorKeying number: '" + idTexture[0]+ "' created.");
					else
						Log.d("debug_java", "Texture colorKeying: '" + this.fileNameTexture+ "' created.");
				}
			}
			imageBuffer.clear();
			this.width  = mBitmap.getWidth();
			this.height = mBitmap.getHeight();
			this.hasColorKeying = mBitmap.hasAlpha();
			mBitmap.recycle();
		}
		//------------------------------------------------------------------------------------------------------------
		public void release(GL10 gl)//libera a textura
		{
			try
			{
				gl.glDeleteTextures(1, idTexture, 0);
				if(InstanceActivityEngine.debugMessage)
				{
					if(this.fileNameTexture!=null)
						Log.d("debug_java", "Texture '" + this.fileNameTexture + "' successfully released!");
					else
						Log.d("debug_java", "Texture '" + this.idTexture[0] + "' successfully released!");
				}
				idTexture = null;
			}
			catch (Exception e) 
			{
				if(InstanceActivityEngine.debugMessage)
					Log.d("debug_java", "error: " + e.toString());
			}
		}
		
		public void setTexture(GL10 gl)
		{
			if(idTexture != null)
			{
				gl.glEnable(GL10.GL_TEXTURE_2D);
				gl.glColor4f(colorMask.r,colorMask.g,colorMask.b,colorMask.a);//Importante habilitar o alpha
				if(hasColorKeying)
				{
					gl.glEnable( GL10.GL_BLEND );
					gl.glBlendFunc( GL10.GL_SRC_ALPHA, GL10.GL_ONE_MINUS_SRC_ALPHA );
				}
				gl.glBindTexture(GL10.GL_TEXTURE_2D,idTexture[0]);
			}
		}
	}
}
