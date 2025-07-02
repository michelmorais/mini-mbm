package com.mini.mbm;

import android.media.SoundPool;
import android.util.Log;

public class SoundJniEngine
{
    public int          soundId;
    private SoundPool   soundPool;
    public String       name;
    public String       fileNameOnLua;
    public float        volume;
    public float        pan;
    public float        pitch;
    public boolean      isPlayingNow;
    public boolean      playOnComplete;
    public final int    indexJNI;

    public SoundJniEngine(SoundPool soundPool,int soundId,final int indexJNI)
    {
        this.soundId    = soundId;
        this.soundPool  = soundPool;
        this.volume     = 1.0f;
        this.pitch      =   0.0f;
        this.isPlayingNow   =   false;
        this.playOnComplete = false;
        this.indexJNI   =   indexJNI;
    }

    public void play()
    {
        int result = soundPool.play(soundId, this.volume, this.volume, 0, 0, 1);
        if(result == 0)
            Log.d("mbm","error on play " + fileNameOnLua);
        this.playOnComplete = true;
    }
    public void resume()
    {
        this.soundPool.autoResume();
    }

    public void stop()
    {
        this.isPlayingNow = false;
        soundPool.stop(soundId);
    }

    public void pause()
    {
        this.isPlayingNow = false;
        soundPool.autoPause();
    }

    public void dispose()
    {
        this.isPlayingNow = false;
        soundPool.unload(soundId);
    }

    public void setVolume(float vol)
    {
        this.volume = vol;
        this.soundPool.setVolume(soundId, this.volume, this.volume);
    }

    public void setPitch(float pitch)
    {
        this.pitch = pitch;
        this.soundPool.setRate(soundId, pitch);
    }
}
