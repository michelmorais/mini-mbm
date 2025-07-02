package com.mini.mbm;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.net.Uri;
import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Vector;

import javax.microedition.khronos.opengles.GL10;

public class FileJniEngine
{
	static public boolean enableRotateBitmap = false;
	static private FileDescriptor 		fd;
	static public long offset;
	static public long len;
	static public int widthImage;
	static public int heightImage;
	static public String fileNameCurrent;
	static byte[] bufferReadFileTmp = new byte[1024];
	static private Vector<String> lsPaths = new Vector<>();

	public FileJniEngine()
	{
		
	}
   	
	private static void mkdirOnFiles(String fileNameFullPath)
	{
		String [] path = fileNameFullPath.split("/");
		String mount = "/";
		if(path.length > 0)
		{
			for(int i=0; i< path.length; i++)
			{
				mount = mount + path[i];
				File f = new File(mount);
		        if(!f.exists() && !f.isFile() && (i+1) != path.length)
				{
					if(!f.mkdir())
						Log.d("debug_java","Failed to create folder: "+mount);
				}
		        if((i+1) < path.length && i != 0)
		        	mount = mount + "/";
			}
		}
	}
	@SuppressWarnings("unused")
	public static String copyFileFromAsset(String fileName,String mode)
	{
		InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
		try
		{
			if(fileName.contains(mainActivity.absPath))
			{
				fileNameCurrent = fileName;
				return fileName;
			}
			else if(fileNameCurrent.length() == 0)
			{
				if(existFileOnAssets(fileName))
					fileName = fileNameCurrent;
			}
			else if(fileNameCurrent.contains(fileName))
			{
				fileName = fileNameCurrent;
			}
			else if(existFileOnAssets(fileName))
			{
				fileName = fileNameCurrent;
			}
			
			InputStream fIn = null;
		    try 
		    {
		    	if(mode.startsWith("r"))
		    	{
		    		fIn = mainActivity.activity.getResources().getAssets().open(fileName, Context.MODE_PRIVATE);
			        String fileNameOut =  mainActivity.absPath  + fileName;
			        File f = new File(fileNameOut);
			        if(f.exists() && f.isFile())
			        {
			        	return fileNameOut;
			        }
			        mkdirOnFiles(fileNameOut);
			        int length;
			        File file = new File(fileNameOut);
			        BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(file));
			        while((length = fIn.read(bufferReadFileTmp)) > 0)
		            {
			        	out.write(bufferReadFileTmp, 0, length);
		            }
			        out.close();
			        try 
			        {
			            fIn.close();
			        } 
			        catch (Exception e2) 
			        {
			        	if(InstanceActivityEngine.debugMessage)
				    		Log.d("debug_java",e2.toString());
			        }
			        return fileNameOut;
		    	}
		    	else//w
		    	{
		    		String fileNameOut =  mainActivity.absPath  + fileName;
		    		mkdirOnFiles(fileNameOut);
			        return fileNameOut;
		    	}
		    } 
		    catch (Exception e) 
		    {
		    	try 
		        {
		            if (fIn != null)
		                fIn.close();
		        } 
		        catch (Exception e2) 
		        {
		        	if(InstanceActivityEngine.debugMessage)
			    		Log.d("debug_java",e2.toString());
		        }
		    	String firstProblem = "File: "+ fileName + " " + e.toString();
		    	if(InstanceActivityEngine.debugMessage)
		    		Log.d("debug_java",firstProblem );
		        return null;
		    }
		}
	    catch (Exception e) 
	    {
	    	if(InstanceActivityEngine.debugMessage)
	    		Log.d("debug_java",e.toString());
	    	return null;
	    }
	}
	
	@SuppressWarnings("unused")
	public static void addPath(String filePath)
	{
		for(int i=0; i< lsPaths.size(); i++)
		{
			if(lsPaths.get(i).compareTo(filePath) == 0)
				return;
		}
		lsPaths.add(filePath);
	}
	
	public static boolean existFileOnAssets(String fileName)
	{
		System.gc();
		try
		{
			File f = new File(fileName);
			if(f.exists())
			{
				fileNameCurrent = f.getAbsolutePath();
				return true;
			}
		}
		catch (Exception exc)
		{

		}
		try
		{
			fileNameCurrent = "";
			InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
			String str[] = mainActivity.lsAssets.get("");

			if (str != null)
			{
				for(int i=0; i< str.length; i++)
				{
					if(fileName.compareTo(str[i]) == 0)
					{
						fileNameCurrent = str[i];
						return true;
					}
				}
			}
			for(int i=0; i< lsPaths.size(); i++ )
			{
				str = mainActivity.lsAssets.get(lsPaths.get(i));
				if(str != null)
				{
					for(int j=0; j< str.length; j++)
					{
						if(fileName.compareTo(str[j]) == 0)
						{
							fileNameCurrent = lsPaths.get(i) + "/" + str[j];
							return true;
						}
						String newFile = str[j] + "/" +  fileName;
						if(newFile.compareTo(str[j]) == 0)
						{
							fileNameCurrent = newFile; 
							return true;
						}
					}
				}
			}
			return false;
		}
		catch (Exception e2) 
		{
			String firstProblem = "File: "+ fileName + " " + e2.toString();
			if(InstanceActivityEngine.debugMessage)
				Log.d("debug_java",firstProblem );
		}
		return false;
	}
	
	public static Bitmap decodeSampledBitmapFromFileName(String fileName,final boolean resize) 
	{
	    // First decode with inJustDecodeBounds=true to check dimensions
	    final BitmapFactory.Options options = new BitmapFactory.Options();
	    options.inJustDecodeBounds = true;
	    Bitmap bRet = BitmapFactory.decodeFile(fileName, options);
	    if(bRet == null)
	    {
	    	try
	    	{
		    	bRet = getThumbnail(fileName,resize);
				return bRet;
	    	}
	    	catch(Exception e)
	    	{
	    		return null;
	    		
	    	}
	    }
	    InstanceActivityEngine instance = InstanceActivityEngine.getInstance();  
	    int widthScreen = instance.widthScreen;
	    
	    int width = options.outWidth;
    	int height = options.outHeight;
    	if(resize)
    	{
	    	for(int i=0; i< 30; i++)
	    	{
	    		int p2 = (int) Math.pow(2, i);
	    		int p3 = (int) Math.pow(2, i+1);
	    		if(width == p2)
	    			break;
	    		if(width > p2 && width < p3 || p2 >= widthScreen)
	    		{
	    			if(p2 >= widthScreen)
	    			{
	    				p2 = (int) Math.pow(2, i-1);
	    			}
	    			double perc = (double)p2 / (double)width;
	    			width = p2;
	    			height = (int)((double)height * perc); 
	    			break;
	    		}
	    	}
    	}
	    // Calculate inSampleSize
	    options.inSampleSize = calculateInSampleSize(options, width, height);
	    // Decode bitmap with inSampleSize set
	    options.inJustDecodeBounds = false;
	    bRet =  BitmapFactory.decodeFile(fileName, options);
		return bRet;
	}
	
	public static int calculateInSampleSize	(BitmapFactory.Options options, int reqWidth, int reqHeight) 
	{
	    // Raw height and width of image
	    final int height = options.outHeight;
	    final int width = options.outWidth;
	    int inSampleSize = 1;
	
	    if (height > reqHeight || width > reqWidth) 
	    {
	        final int halfHeight = height / 2;
	        final int halfWidth = width / 2;
	        // Calculate the largest inSampleSize value that is a power of 2 and keeps both
	        // height and width larger than the requested height and width.
	        while ((halfHeight / inSampleSize) > reqHeight
	                && (halfWidth / inSampleSize) > reqWidth) 
	        {
	            inSampleSize *= 2;
	        }
	    }
	    return inSampleSize;
	}
	
	@SuppressWarnings("unused")
	public static String getFullPath(String fileName)
	{
		if(fileName!= null)
		{
			InstanceActivityEngine ac = InstanceActivityEngine.getInstance();
			final int len = Math.min(ac.absPath.length(),fileName.length());
			for(int i = 0; i< len; i++)
			{
				final char c1 =  fileName.charAt(i);
				final char c2 = ac.absPath.charAt(i);
				if(c1 != c2)
					return  ac.absPath + fileName;
			}
		}
		return fileName;
	}
	
	@SuppressWarnings("unused")
	public static byte [] getBytesImage(String fileName) 
	{
		try
		{
			if(fileNameCurrent.length() == 0)
			{
				if(existFileOnAssets(fileName))
					fileName = fileNameCurrent;
			}
			else if(fileNameCurrent.contains(fileName))
			{
				fileName = fileNameCurrent;
			}
			else if(existFileOnAssets(fileName))
			{
				fileName = fileNameCurrent;
			}
			widthImage  = 0;
			heightImage = 0;
			GL10 gl = InstanceActivityEngine.getInstance().gl10;
			TextureManagerJniEngine texMan = TextureManagerJniEngine.getInstance(gl);
            TextureManagerJniEngine.TEXTURE texture = texMan.load(gl, fileName);
			if(texture != null)
			{
				int id[] = texture.getIdTexture();
				byte image[] = new byte [4+1];//Id texture + boolean alpha color
				image[0]	=	(byte)((id[0] & 0xff000000) >> 24);
				image[1]	=	(byte)((id[0] & 0x00ff0000) >> 16);
				image[2]	=	(byte)((id[0] & 0x0000ff00) >> 8 );
				image[3]	=	(byte)((id[0] & 0x000000ff)      );
				image[4]	=	(byte) (texture.hasColorKeying ? 1 : 0);
				int ret     =  (((int)image[0])<<24) | (((int)image[1])<<16) | (((int)image[2])<<8) | (((int)image[3]));
				if(ret != id[0])
				{
					if(InstanceActivityEngine.debugMessage)
						Log.d("debug_java", "Whata");
				}
				widthImage  = -texture.getWidth();
				heightImage = -texture.getHeight();
				return image;
				
			}
			else
			{
				Bitmap mBitmap = decodeSampledBitmapFromFileName(fileName,true);
				if(mBitmap == null)
				{
					mBitmap = decodeSampledBitmapFromFileName(fileName,false);
					if(mBitmap == null)
						return null;
				}
				mBitmap = rotateBitmap(mBitmap);

				final int h = mBitmap.getHeight();
				final int w = mBitmap.getWidth();
				len			=	w * h * 3;
				byte image[] = new byte [(int) len];
				for ( int y = 0, i = 0; y < h; y++ )
				{
				    for ( int x = 0; x < w; x++ )
				    {
				    	final int pixel 	= mBitmap.getPixel(x, y);//0XAABBGGRR
					    final int red   	= (pixel >> 16) & 0xFF;//r
					    final int green 	= (pixel >> 8 ) & 0xFF;//g
					    final int blue  	= (pixel)       & 0xFF;//b
		                image[i]			=	(byte)red;
		                image[i+1]			=	(byte)green;
		                image[i+2]			=	(byte)blue;
		                i+=3;
				    }
				}
				mBitmap.recycle();
				widthImage  = w;
				heightImage = h;
				return image;
			}
		}
		catch (Exception e2) 
		{
			return null;
		}
	}
	
	public static void deleteFileTemp(final String fileName)
	{
		try
		{
			InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
			mainActivity.activity.deleteFile(fileName);
		}
		catch (Exception e) 
		{
			Log.d("debug_java","Failed 'deleteFileTemp':"+e.toString());
		}
	}
	
	public static FileDescriptor getTempFileDescriptor(String fileName) 
	{
		InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
	    InputStream fIn = null;
	    InputStreamReader isr = null;
	    BufferedReader input = null;
	    try 
	    {
	        fIn = mainActivity.activity.getResources().getAssets().open(fileName, Context.MODE_PRIVATE);
	        isr = new InputStreamReader(fIn);
	        input = new BufferedReader(isr);
	        String fileNameTemp = "file_TXT_tmp.txt";
	        deleteFileTemp(fileNameTemp);
	        int length;
	        FileOutputStream out = mainActivity.activity.openFileOutput(fileNameTemp, Context.MODE_APPEND);
	        while((length = fIn.read(bufferReadFileTmp)) > 0)
            {
	        	out.write(bufferReadFileTmp, 0, length);
            }
	        
	        out.close();
	        try 
	        {
				isr.close();
				fIn.close();
				input.close();
	        } 
	        catch (Exception e2) 
	        {
	           Log.e("debug_java","getTempFileDescriptor:"+e2.toString());
	        }
	        FileInputStream in = mainActivity.activity.openFileInput(fileNameTemp);
			offset = 0;
			len = (long) in.available();
			fd = in.getFD();
			return fd;
	    } 
	    catch (Exception e) 
	    {
	    	try 
	        {
	            if (isr != null)
	                isr.close();
	            if (fIn != null)
	                fIn.close();
	            if (input != null)
	                input.close();
	        } 
	        catch (Exception e2) 
	        {
				Log.e("debug_java","getTempFileDescriptor:"+e2.toString());
	        }
	    	String firstProblem = "File: "+ fileName + " " + e.toString();
	    	if(InstanceActivityEngine.debugMessage)
	    		Log.d("debug_java",firstProblem );
	        return null;
	    } 
	}


	/*
	Android compresses all assets, except for the following types:
	
	".jpg", ".jpeg", ".png", ".gif",
	".wav", ".mp2", ".mp3", ".ogg", ".aac",
	".mpg", ".mpeg", ".mid", ".midi", ".smf", ".jet",
	".rtttl", ".imy", ".xmf", ".mp4", ".m4a",
	".m4v", ".3gp", ".3gpp", ".3g2", ".3gpp2",
	".amr", ".awb", ".wma", ".wmv"
	 
	 * */
	@SuppressWarnings("unused")
 	public static FileDescriptor openFD(String fileName)
	{
		InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
		try
		{
			if(fileNameCurrent.length() == 0)
			{
				if(existFileOnAssets(fileName))
					fileName = fileNameCurrent;
			}
			else if(fileNameCurrent.contains(fileName))
			{
				fileName = fileNameCurrent;
			}
			else if(existFileOnAssets(fileName))
			{
				fileName = fileNameCurrent;
			}
			
			offset	=	0;
			len		=	0;
			AssetFileDescriptor afd = 	mainActivity.getAssets().openFd(fileName);
			fd = afd.getFileDescriptor();
			offset = afd.getStartOffset();
			len = afd.getLength();
			return fd;
		}
		catch (Exception e) 
		{
			try
			{
				fd = getTempFileDescriptor(fileName);
				return fd;
			}
			catch (Exception e2) 
			{
				String firstProblem = "File: "+ fileName + " " + e.toString();
				String secProblem = "File: "+ fileName + " " + e2.toString();
				if(InstanceActivityEngine.debugMessage)
				{
					Log.d("debug_java",firstProblem );
					Log.d("debug_java",secProblem );
				}
			}
		}
		return null;
	}

	public static Bitmap rotateBitmap(Bitmap b)
	{
		if(b == null)
			return null;
		if(!enableRotateBitmap)
			return b;
		final int width = b.getWidth();
		final int height = b.getHeight();
		if(width > height)
			return b;
		Bitmap myBitmap;
		try
		{
			Matrix matrix = new Matrix();
			matrix.postRotate(90);
			myBitmap = Bitmap.createBitmap(b, 0, 0, width, height, matrix, true);//rotating bitmap
		}
		catch (Exception e)
		{
			Log.d("debug_java", e.toString());
			return b;
		}
		b.recycle();
		return myBitmap;
	}
	public static Bitmap getBitmapFromAsset(String path)
	{
		Bitmap bitmap = null;
		try
		{
			InputStream inputStream = null;
			try
			{
				InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
				inputStream = instance.getAssets().open(path);
				bitmap = BitmapFactory.decodeStream(inputStream);
			}
			catch (Exception e)
			{
				bitmap = null;
			}
			finally
			{
				if(inputStream!= null)
					inputStream.close();
			}
		}
		catch (Exception e)
		{

		}
		return bitmap;
	}
	public static Bitmap getThumbnail(String fileName,boolean resize)
	{
		try
		{
			Bitmap bRetAsset =  getBitmapFromAsset(fileName);
			if(bRetAsset != null)
				return bRetAsset;
			InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
			Uri uri = instance.lsUri.get(fileName);
			final int widthScreen = instance.widthScreen;
			if(uri == null)
			{
				BitmapFactory.Options onlyBoundsOptions = new BitmapFactory.Options();
		        onlyBoundsOptions.inJustDecodeBounds = true;
		        onlyBoundsOptions.inDither=true;//optional
		        onlyBoundsOptions.inPreferredConfig=Bitmap.Config.ARGB_8888;//optional
		        BitmapFactory.decodeFile(fileName, onlyBoundsOptions);
		        if ((onlyBoundsOptions.outWidth == -1) || (onlyBoundsOptions.outHeight == -1))
		            return null;
		        int width = onlyBoundsOptions.outWidth;
		    	int height = onlyBoundsOptions.outHeight;
		    	if(resize)
		    	{
		    		GL10 gl = InstanceActivityEngine.getInstance().gl10;
					TextureManagerJniEngine texMan = TextureManagerJniEngine.getInstance(gl);
		    		final int maxTextureSize = texMan.getMaxTextureSize();
			    	for(int i=0; i< 30; i++)
			    	{
			    		int p2 = (int) Math.pow(2, i);
			    		int p3 = (int) Math.pow(2, i+1);
			    		if(width == p2)
			    			break;
			    		if(p2 < maxTextureSize &&  
			    			width > p2 && 
			    			(width < p3 || p2 >= widthScreen))
			    		{
			    			if(p2 >= widthScreen)
			    			{
			    				p2 = (int) Math.pow(2, i-1);
			    			}
			    			double perc = (double)p2 / (double)width;
			    			width = p2;
			    			height = (int)((double)height * perc);
			    			if(height < maxTextureSize)
			    				break;
			    		}
			    	}
		    	}
			    BitmapFactory.Options bitmapOptions = new BitmapFactory.Options();
		        bitmapOptions.inSampleSize = calculateInSampleSize(onlyBoundsOptions, width, height);
		        bitmapOptions.inDither=true;//optional
		        bitmapOptions.inPreferredConfig=Bitmap.Config.ARGB_8888;//optional
				return BitmapFactory.decodeFile(fileName, bitmapOptions);
			}
			else
			{
				InputStream input = instance.activity.getContentResolver().openInputStream(uri);
		        BitmapFactory.Options onlyBoundsOptions = new BitmapFactory.Options();
		        onlyBoundsOptions.inJustDecodeBounds = true;
		        onlyBoundsOptions.inDither=true;//optional
		        onlyBoundsOptions.inPreferredConfig=Bitmap.Config.ARGB_8888;//optional
		        BitmapFactory.decodeStream(input, null, onlyBoundsOptions);
		        input.close();
		        if ((onlyBoundsOptions.outWidth == -1) || (onlyBoundsOptions.outHeight == -1))
		            return null;
		        int width = onlyBoundsOptions.outWidth;
		    	int height = onlyBoundsOptions.outHeight;
		    	if(resize)
		    	{
		    		GL10 gl = InstanceActivityEngine.getInstance().gl10;
					TextureManagerJniEngine texMan = TextureManagerJniEngine.getInstance(gl);
		    		final int maxTextureSize = texMan.getMaxTextureSize();
			    	for(int i=0; i< 30; i++)
			    	{
			    		int p2 = (int) Math.pow(2, i);
			    		int p3 = (int) Math.pow(2, i+1);
			    		if(width == p2)
			    			break;
			    		if(p2 < maxTextureSize &&  
			    			width > p2 && 
			    			(width < p3 || p2 >= widthScreen))
			    		{
			    			if(p2 >= widthScreen)
			    			{
			    				p2 = (int) Math.pow(2, i-1);
			    			}
			    			double perc = (double)p2 / (double)width;
			    			width = p2;
			    			height = (int)((double)height * perc);
			    			if(height < maxTextureSize)
			    				break;
			    		}
			    	}
		    	}
			    BitmapFactory.Options bitmapOptions = new BitmapFactory.Options();
		        bitmapOptions.inSampleSize = calculateInSampleSize(onlyBoundsOptions, width, height);
		        bitmapOptions.inDither=true;//optional
		        bitmapOptions.inPreferredConfig=Bitmap.Config.ARGB_8888;//optional
		        input = instance.activity.getContentResolver().openInputStream(uri);
		        Bitmap bitmap = BitmapFactory.decodeStream(input, null, bitmapOptions);
		        input.close();
		        return bitmap;
			}
	    	
		}
		catch(Exception e)
		{
			return null;
		}
    }
}
