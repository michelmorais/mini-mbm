package com.mini.mbm;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.util.HashMap;
import javax.microedition.khronos.opengles.GL10;
import android.app.Activity;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;
import android.util.Log;
import android.util.SparseArray;

public class InstanceActivityEngine 
{
	public 	Activity 					activity;
	private	AssetManager 				assets;
	private static InstanceActivityEngine 	mainActivity = null;
	public static boolean 				run = true;
	public ViewJniEngine 						view;
	public int 							widthScreen  = 0;
	public int 							heightScreen = 0;
	public HashMap<String, Uri>			lsUri;
	public GL10							gl10;
	public HashMap<String, String[]>	lsAssets;
	public final String 				absPath;
	public final String 				apkPath;
	public static boolean				debugMessage = false;
	public SparseArray<String> 			keyNames;
	//--------------------------------------------------------------------------------------------------------
	private InstanceActivityEngine(String absPath,String apkPath)
	{
		this.activity 	= null;
		this.assets		= null;
		this.view		= null;
		this.lsUri		= new HashMap<>();
		this.lsAssets	= new HashMap<>();
		this.absPath 	= absPath;
		this.apkPath 	= apkPath;
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB_MR1)
		{
			this.keyNames = new SparseArray<>();
			KeyCodeJniEngine.buildMapKey(this.keyNames);
		}
	}
	//--------------------------------------------------------------------------------------------------------
	public static InstanceActivityEngine getInstance(Activity newActivity,final String absPath,final String apkPath,final String version)
	{
		if(mainActivity==null)
			mainActivity 		= new InstanceActivityEngine(absPath,apkPath);
		if(newActivity != null)
		{
			mainActivity.activity	= 	newActivity;
			if(mainActivity.lsAssets.size() == 0)
			{
				mainActivity.updateAssets();
				final String fileNameVersion = mainActivity.absPath + "version.txt";
				if(mainActivity.needUpdateFilesBYRevision(fileNameVersion,version))
				{
					mainActivity.removeFilesExistents(fileNameVersion);
					mainActivity.createVersionFile(fileNameVersion,version);
				}
			}
		}
		return mainActivity;
	}
	//--------------------------------------------------------------------------------------------------------
	private void createVersionFile(final String fileName,final String version) 
	{
		try 
        {
			File file = new File(fileName);
			BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(file));
	        byte [] bVersion =version.getBytes();  
	        out.write(bVersion);
	        out.close();
        }
        catch (Exception e2) 
        {
        	if(InstanceActivityEngine.debugMessage)
	    		Log.d("debug_java",e2.toString());
        }
	}
	//--------------------------------------------------------------------------------------------------------
	private boolean needUpdateFilesBYRevision(final String fileName,final String version)  
	{
		boolean needUpdate = true;
		try 
		{
			BufferedReader br = new BufferedReader(new FileReader(fileName));
			String line;
			while ((line = br.readLine()) != null) 
			{
			   if(line.compareTo(version) == 0)
			   {
				   needUpdate = false;
				   break;
			   }
			}
			br.close();
		} 
		catch (Exception e) 
		{
			if(InstanceActivityEngine.debugMessage)
				Log.d("debug_java",e.toString());
		}
		return needUpdate;
	}
	//--------------------------------------------------------------------------------------------------------
	public static InstanceActivityEngine getInstance()
	{
		return mainActivity;
	}
	//--------------------------------------------------------------------------------------------------------
	private void removeFilesExistents(final String fileNameVersion)
	{
		String str[] = this.lsAssets.get("");
		if (str != null)
		{
			for(int i=0; i< str.length; i++)
			{
				String fileName = this.absPath + str[i];
				File f = new File(fileName);
		        deleteRecursive(f);
			}
		}
		File fileVersion = new File(fileNameVersion);
		if(fileVersion.delete())
	    	Log.i("debug_java","File ["+fileVersion.getName()+"] "+"updated successfully!");
	}
	//--------------------------------------------------------------------------------------------------------
	void deleteRecursive(File fileOrDirectory) 
	{
		if (fileOrDirectory.isDirectory())
		{
			for (File child : fileOrDirectory.listFiles())
			{
				try
	    		{
					deleteRecursive(child);
	    		}
				catch (Exception e) 
	    		{
	    			if(InstanceActivityEngine.debugMessage)
	    	    		Log.d("debug_java",e.toString());
	    		}
			}
		}
	    if(fileOrDirectory.delete())
	    	Log.i("debug_java","File ["+fileOrDirectory.getName()+"] "+"updated successfully!");
	}
	//--------------------------------------------------------------------------------------------------------
	private void populateSubDir(AssetManager assets, String path)
	{
		try
		{
			String str[] = assets.list(path);
			if(str.length > 0)
			{
				this.lsAssets.put(path, str);
				for (int i = 0; i < str.length; i++)
				{
					String currentPath = str[i];
					String combinedPath = path + '/' + currentPath;
					String strSub[] = assets.list(combinedPath);
					for (int j = 0; j < strSub.length; j++)
					{
						if(this.lsAssets.containsKey(combinedPath) == false)
							populateSubDir(assets,combinedPath );
					}
				}
			}
		}
		catch(Exception e)
		{
			e.printStackTrace();
		}
	}
	//--------------------------------------------------------------------------------------------------------
	private void updateAssets()
	{
		try
		{
			assets 	= activity.getAssets();
			if(this.lsAssets.size() == 0)
			{
				String str[] = assets.list("");
				this.lsAssets.put("", str);
				for(int i=0;i< str.length; i++)
				{
					String currentPath = str[i];
					populateSubDir(assets,currentPath);
				}
			}
		}
		catch(Exception e)
		{
			e.printStackTrace();
		}
	}
	//--------------------------------------------------------------------------------------------------------
	public	AssetManager getAssets()
	{
		return this.assets;
	}
}
