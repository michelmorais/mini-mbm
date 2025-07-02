package com.mini.mbm;
import android.annotation.SuppressLint;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.SoundPool;
import android.media.SoundPool.OnLoadCompleteListener;
import android.os.Build;
import android.util.Log;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;


public class AudioManagerJniEngine implements OnLoadCompleteListener
{
	public SoundPool 				soundPool;
	private static int indexCount		=	0;

	private static	AudioManagerJniEngine			instanceAudioManager	=	null;
	private HashMap<Integer, AudioJniEngine>		lsAudioMbmJni;

    @SuppressLint("UseSparseArrays")
	private AudioManagerJniEngine(final int simultaneousStreams )
	{
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
		{
		    SoundPool.Builder b = new SoundPool.Builder();
		    AudioAttributes attr = new AudioAttributes.Builder()
		            .setUsage(AudioAttributes.USAGE_GAME)
		            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
		            .build();
		    b.setMaxStreams(simultaneousStreams);
		    b.setAudioAttributes(attr);
		    this.soundPool = b.build();
		}
		else
		{
		    this.soundPool = new SoundPool(simultaneousStreams, AudioManager.STREAM_MUSIC, 0);
		}
		this.soundPool.setOnLoadCompleteListener(this);
			this.lsAudioMbmJni 	= new HashMap<>();
	}
	
	public static AudioManagerJniEngine getInstance()
	{
		if(instanceAudioManager == null)
		{
			instanceAudioManager = new AudioManagerJniEngine(30);
		}
		return instanceAudioManager;
	}
	
	public static AudioManagerJniEngine getInstance(final int simultaneousStreams)
	{
		if(instanceAudioManager == null)
		{
			instanceAudioManager = new AudioManagerJniEngine(simultaneousStreams);
		}
		return instanceAudioManager;
	}
	
	@SuppressWarnings("unused")
	public static int onNewAudioJniEngine()
	{
		instanceAudioManager = getInstance();
		AudioJniEngine audio = new AudioJniEngine(instanceAudioManager.getIndexCount());
		instanceAudioManager.lsAudioMbmJni.put(audio.indexJNI,audio);
		AudioManagerJniEngine.indexCount++;
		return audio.indexJNI;
	}
	
	public final int getIndexCount()
	{
		return AudioManagerJniEngine.indexCount;
	}
	
	public static AudioJniEngine getAudioJniEngine(final int indexJNI)
	{
		if(instanceAudioManager == null)
			return null;
		return instanceAudioManager.lsAudioMbmJni.get(indexJNI);
	}
	
	@SuppressWarnings("unused")
	public static void onDestroyAudioJniEngine(int indexJNI)
	{
		if(instanceAudioManager == null)
			return;
		AudioJniEngine audioJni = instanceAudioManager.lsAudioMbmJni.get(indexJNI);
		if(audioJni != null)
		{
			audioJni.destroy();
			instanceAudioManager.lsAudioMbmJni.remove(indexJNI);
		}
	}
	
	@SuppressWarnings("unused")
	public static boolean onLoadAudioJniEngine(int indexJNI,String fileName,boolean loop,boolean inMemory)
	{
		final String fileNameOnLua = fileName;
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni == null)
			return false;
		if(fileName.length() == 0)
		{
			if(FileJniEngine.existFileOnAssets(fileName))
				fileName = FileJniEngine.fileNameCurrent;
		}
		else if(FileJniEngine.fileNameCurrent.compareTo(fileName) == 0)
		{
			fileName = FileJniEngine.fileNameCurrent;
		}
		else if(FileJniEngine.existFileOnAssets(fileName))
		{
			fileName = FileJniEngine.fileNameCurrent;
		}
		boolean ret = audioJni.load(fileName,fileNameOnLua,inMemory);
		if(ret)
			audioJni.setLooping(loop);
		return ret;
	}
	
	@SuppressWarnings("unused")
	public static boolean onPlayAudioJniEngine(int indexJNI,boolean loop)//
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			return audioJni.play(loop);
		return false;
	}
	
	@SuppressWarnings("unused")
	public static void onSetVolumeAudioJniEngine(int indexJNI,float volume)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			audioJni.setVolume(volume);
	}
	
	@SuppressWarnings("unused")
	public static void onSetPanAudioJniEngine(int indexJNI,float pan)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			audioJni.setPan(pan);
	}
	
	@SuppressWarnings("unused")
	public static void onPauseAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			audioJni.pause();
	}
	
	@SuppressWarnings("unused")
	public static void onResumeAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			audioJni.resume();
	}
	
	@SuppressWarnings("unused")
	public static void onStopAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			audioJni.stop();
	}
	
	@SuppressWarnings("unused")
	public static void onSetPitchAudioJniEngine(int indexJNI,float pitch)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			audioJni.setPitch(pitch);
	}
	
	@SuppressWarnings("unused")
	public static boolean onIsPlayingAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		return audioJni != null && audioJni.isPlaying();
	}
	
	@SuppressWarnings("unused")
	public static boolean onIsPausedAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		return audioJni != null && audioJni.isPaused();
	}
	
	@SuppressWarnings("unused")
	public static float onGetVolumeAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			return audioJni.getVolume();
		return 0.0f;
	}
	
	@SuppressWarnings("unused")
	public static float onGetPanAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			return audioJni.getPan();
		return 0.0f;
	}
	
	@SuppressWarnings("unused")
	public static float onGetPitchAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			return audioJni.getPitch();
		return 0.0f;
	}
	
	@SuppressWarnings("unused")
	public static int onGetLengthAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			return audioJni.getLength();
		return 0;
	}
	
	@SuppressWarnings("unused")
	public static void onResetAudioJniEngine(int indexJNI)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			audioJni.stop();
	}
	
	@SuppressWarnings("unused")
	public static void onSetPositionAudioJniEngine(int indexJNI,int pos)
	{
		AudioJniEngine audioJni = getAudioJniEngine(indexJNI);
		if(audioJni != null)
			audioJni.seek(pos);
	}
	
	@Override
	public void onLoadComplete(SoundPool soundPool, int sampleId, int status)
	{
		try 
		{
			if (status == 0) 
			{
				boolean found = false;
				for (Integer key : instanceAudioManager.lsAudioMbmJni.keySet())
				{
					AudioJniEngine audio = 	instanceAudioManager.lsAudioMbmJni.get(key);
					SoundJniEngine sound = audio.getSoundById(sampleId);
					if (sound != null && sound.playOnComplete) {
						sound.isPlayingNow = true;
						sound.playOnComplete = false;
						sound.play();
						found = true;
						break;
					}
				}

				if(found == false)
					Log.d("debug_java","MBM: Not found id  " + sampleId + " in the sound list size:" + instanceAudioManager.lsAudioMbmJni.size());
			}
		}
		catch (Exception e)
		{
			Log.d("mbm",e.getMessage());
		}
	}
}
