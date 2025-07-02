package com.mini.mbm;

import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnCompletionListener;
import java.io.IOException;

public class MusicJniEngine implements OnCompletionListener
{
    public  MediaPlayer mediaPlayer;
    public String       fileNameOnLua;
    public float        volume;
    private boolean     prepared;
    public final int    indexJNI;
    
    public MusicJniEngine(AssetFileDescriptor assetDescriptor,final int  myIndexJNI)
    {
        indexJNI = myIndexJNI;
        mediaPlayer = new MediaPlayer();
        try
        {
            mediaPlayer.setDataSource(assetDescriptor.getFileDescriptor(),
                    assetDescriptor.getStartOffset(),
                    assetDescriptor.getLength());
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP)
            {
                final AudioAttributes aa = new AudioAttributes.Builder().
                        setUsage(AudioAttributes.USAGE_GAME).
                        setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).
                        build();
                mediaPlayer.setAudioAttributes(aa);
            }
            else
            {
                mediaPlayer.setAudioStreamType(AudioManager.STREAM_MUSIC);
            }

            mediaPlayer.setOnCompletionListener(this);
            mediaPlayer.prepare();
            prepared = true;
        }
        catch (Exception e)
        {
            throw new RuntimeException("Error on load music");
        }
        volume = 1.0f;
    }
    
    public void dispose()
    {
        if (mediaPlayer.isPlaying())
            mediaPlayer.stop();
        mediaPlayer.release();
    }
    
    @SuppressWarnings("unused")
    public boolean isLooping()
    {
        return mediaPlayer.isLooping();
    }
    
    public boolean isPlaying()
    {
        return mediaPlayer.isPlaying();
    }
    
    @SuppressWarnings("unused")
    public boolean isStopped()
    {
        return !mediaPlayer.isPlaying();
    }
    
    public void pause()
    {
        if (mediaPlayer.isPlaying())
        {
            mediaPlayer.pause();
        }
    }
    
    public void resume()
    {
        try
        {
            synchronized (this)
            {
                if(prepared == false)
                {
                    mediaPlayer.prepare();
                    prepared = true;
                }
                mediaPlayer.start();
            }
        }
        catch (IllegalStateException | IOException e)
        {
            e.printStackTrace();
        }
    }
    
    public void play()
    {
        if (!mediaPlayer.isPlaying())
        {
            try
            {
                synchronized (this)
                {
                    if(prepared == false)
                    {
                        mediaPlayer.prepare();
                        prepared = true;
                    }
                    mediaPlayer.start();
                }
            }
            catch (IllegalStateException | IOException e)
            {
                e.printStackTrace();
            }
        }
    }
    
    @SuppressWarnings("unused")
    public void setLooping(boolean value)
    {
        mediaPlayer.setLooping(value);
    }
    
    public void setVolume(final float volume)//0 a 1
    {
        this.volume = volume;
        mediaPlayer.setVolume(volume, volume);
    }
    
    public void stop()
    {
        mediaPlayer.stop();
        prepared = false;
    }
    
    public int getLength()
    {
        return mediaPlayer.getDuration();
    }
    
    public void seek(int pos)
    {
        mediaPlayer.seekTo(pos);
    }
    
    public void onCompletion(MediaPlayer mp)
    {
        if(!mp.isLooping())
        {
            InstanceActivityEngine instance = InstanceActivityEngine.getInstance();
            if (instance.view != null)
                instance.view.renderer.OnStreamStopped(indexJNI);
            else
                MiniMbmEngine.streamStopped(indexJNI);
        }
    }
}
