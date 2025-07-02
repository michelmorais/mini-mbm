package com.mini.mbm;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.Log;

public class DoCommandsJniEngine
{
	public static final int SELECT_PICTURE = 8182;
	public static String strFunctionCallBack = null;
	
	@SuppressWarnings("unused")
	public static void getImage(String functionCallBack)
	{
		try
		{
			InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
			DoCommandsJniEngine.strFunctionCallBack = functionCallBack;
			if(instance != null && instance.view != null)
			{
				final Intent galleryIntent = new Intent(Intent.ACTION_PICK,
						android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
				galleryIntent.setType("image/*");
				final Intent chooserIntent = Intent.createChooser(galleryIntent,"Open image...");
		        chooserIntent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
		        instance.activity.startActivityForResult(chooserIntent, SELECT_PICTURE);
		        
			}
		}
		catch(Exception e)
		{
			Log.d("debug_java","getImage:" + e.toString());
		}
	}
	
	@SuppressWarnings("unused")
	public static int getAPILevel()
	{
		return Build.VERSION.SDK_INT;
	}
	
	@SuppressWarnings("unused")
	public static String getIdiom()
	{
		final String lang = Locale.getDefault().toString();//---> pt_BR
		return lang.replace("_", "-");//---> pt-BR
	}
	
	@SuppressWarnings("unused")
	public static void vibrate(int milliseconds)
	{
		InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
		Vibrator v = (Vibrator) instance.activity.getSystemService(Context.VIBRATOR_SERVICE);

    	if (Build.VERSION.SDK_INT >= 26) 
    	{
			VibrationEffect effect = VibrationEffect.createOneShot( milliseconds, 255);
			v.vibrate(effect);
        } 
        else 
        {
			v.vibrate(milliseconds);
        }
	}
    
	@SuppressWarnings("unused")
    public static String OnDoCommands(String parm1,String parm2)
    {
        InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
        MainJniEngine that = (MainJniEngine) mainActivity.activity;
        if(that!= null)
            return that.OnDoCommands(parm1, parm2);
        return null;
    }
	
	@SuppressWarnings("unused")
	public static String getUserName()
	{
		InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
		MainJniEngine that = (MainJniEngine) mainActivity.activity;
		AccountManager manager = AccountManager.get(that);
		Account[] accounts = manager.getAccountsByType("com.google");
		List<String> possibleEmails = new LinkedList<String>();
		for (Account account : accounts)
		{
			possibleEmails.add(account.name);
		}
		if (!possibleEmails.isEmpty() && possibleEmails.get(0) != null)
		{
			String email = possibleEmails.get(0);
			String[] parts = email.split("@");

			if (parts.length > 1)
				return parts[0];
		}
		return null;
	}
	
}

