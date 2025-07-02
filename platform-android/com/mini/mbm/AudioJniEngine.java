package com.mini.mbm;
import java.io.IOException;
import android.content.res.AssetFileDescriptor;
import android.util.Log;


public class AudioJniEngine
{
	
	private MusicJniEngine			m_music;
	private SoundJniEngine			m_sound;
	public final int				indexJNI;
	
	public AudioJniEngine(final int indexJNI)
	{
		this.indexJNI= indexJNI;
		this.m_music = null;
		this.m_sound = null;
	}
	
	public SoundJniEngine getSoundById(int id)
	{
		if(m_sound != null && m_sound.soundId == id)
			return m_sound;
		return null;
	}
	
	public boolean load(final String filename,final String fileNameOnLua,final boolean inMemory)
	{
		if(inMemory)
		{
			SoundJniEngine sound = this.getNewSound(filename,fileNameOnLua);
			return sound != null;
		}
		else
		{
			MusicJniEngine music = this.getNewMusic(filename,fileNameOnLua);
			return music != null;
		}
	}
	
	private MusicJniEngine getNewMusic(final String filename,final String fileNameOnLua)
	{
		try
		{
			InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
			AssetFileDescriptor assetDescriptor = mainActivity.getAssets().openFd(filename);
			m_music = new MusicJniEngine(assetDescriptor,indexJNI);
			m_music.fileNameOnLua = fileNameOnLua;
			if(InstanceActivityEngine.debugMessage)
				Log.d("debug_java","Music '" + m_music.fileNameOnLua+ "' successfully loaded!'");
			return m_music;
		}
		catch (Exception e)
		{
			if(InstanceActivityEngine.debugMessage)
				Log.d("debug_java","Error on load audio '" + filename + "'");
			return null;
		}
	}
	
	private SoundJniEngine getNewSound(final String filename,final String fileNameOnLua)
	{
		if(m_sound != null)
			return m_sound;
		AudioManagerJniEngine audiMan = AudioManagerJniEngine.getInstance();
		try
		{
			InstanceActivityEngine mainActivity = InstanceActivityEngine.getInstance();
			AssetFileDescriptor assetDescriptor = mainActivity.getAssets().openFd(filename);
			int soundId =  audiMan.soundPool.load(assetDescriptor, 0);
			m_sound = new SoundJniEngine(audiMan.soundPool, soundId, indexJNI);
			m_sound.name = filename;
			m_sound.fileNameOnLua = fileNameOnLua;
			if(InstanceActivityEngine.debugMessage)
				Log.d("debug_java","Sound '" + m_sound.fileNameOnLua+ "' successfully loaded!");
			return m_sound;
		}
		catch (IOException e)
		{
			if(InstanceActivityEngine.debugMessage)
				Log.d("debug_java","Error on load stream: '" + filename + "'");
			return null;
		}
	}
	
	public void pause()
	{
		if(m_sound != null )
			m_sound.pause();
		else if(m_music != null)
			m_music.pause();
	}
	
	public void resume()
	{
		if(m_sound != null )
			m_sound.resume();
		else if(m_music != null)
			m_music.resume();
	}
	
	public void destroy()
	{
		if(m_sound != null ) {
			m_sound.dispose();
			m_sound = null;
		}
		else if(m_music != null) {
			m_music.dispose();
			m_music = null;
		}
	}
	
	public void setVolume(float volume)
	{
		if(m_sound != null )
			m_sound.setVolume(volume);
		else if(m_music != null)
			m_music.setVolume(volume);
	}
	
	public void setPan(float pan)
	{
		if(m_sound != null )
			m_sound.pan = pan;//TODO
	}
	
	public void stop()
	{
		if(m_sound != null )
			m_sound.stop();
		else if(m_music != null)
			m_music.stop();
	}
	
	public void setPitch(float pitch)
	{
		if(m_sound != null )
			m_sound.setPitch(pitch);
		//else if(m_music != null)
		//	m_music.setPitch(pitch);
	}
	
	public boolean isPlaying()
	{
		if(m_sound != null )
			return m_sound.isPlayingNow;
		else if(m_music != null)
			return m_music.isPlaying();
		return false;
	}
	
	public boolean isPaused()
	{
		if(m_sound != null )
			return m_sound.isPlayingNow == false;
		else if(m_music != null)
			return m_music.isPlaying() == false;
		return false;
	}
	
	public float getVolume()
	{
		if(m_music != null)
			return m_music.volume;
		if(m_sound != null)
			return m_sound.volume;
		return 0.0f;
	}
	
	public float getPan()
	{
		if(m_sound != null)
			return m_sound.pan;
		return 0.0f;
	}
	
	public float getPitch()
	{
		if(m_sound != null)
			return m_sound.pitch;
		return 0.0f;
	}
	
	public int getLength()
	{
		if(m_music != null)
			return m_music.getLength();
		return 0;
	}
	
	public void seek(final int pos)
	{
		if(m_music != null)
			m_music.seek(pos);
	}

	public void setLooping(boolean loop) {
		if(m_music != null)
			m_music.setLooping(loop);
	}

	public boolean play(boolean loop) {
		if(m_music != null) {
			m_music.play();
			m_music.setLooping(loop);
			return true;
		}
		if(m_sound != null) {
			m_sound.play();
			return true;
		}
		return false;
	}
}
