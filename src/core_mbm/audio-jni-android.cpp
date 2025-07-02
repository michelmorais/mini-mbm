/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#include <audio.h>
#include <device.h>
#include <core-manager.h>
#include <util-interface.h>
#include <platform/common-jni.h>

#if !defined(AUDIO_ENGINE_ANDROID_JNI)
    #error attempt to use AUDIO JNI without define AUDIO_ENGINE_ANDROID_JNI
#endif


namespace mbm
{
    AUDIO::AUDIO(const int newIdScene):AUDIO_INTERFACE(newIdScene),
    onEndStreamCallBack(nullptr)
    {
        int retIndexJni = -1;
        if (onNew_AudioJniEngine(&retIndexJni))
            indexJNI = retIndexJni;
        else
            indexJNI = -1;
    }

    AUDIO::~AUDIO()
    {
		const char *methodName = "onDestroyAudioJniEngine";
		const char *signature = "(I)V"; // void (int)

		util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
		JNIEnv *         jenv = cJni->jenv;
		jmethodID        mid = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
		if (mid == nullptr)
        {
			PRINT_IF_DEBUG("method not found: %s", methodName);
        }
		else
        {
			jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, indexJNI);
        }
		fileName.clear();
    }
        
    bool AUDIO::setVolume(const float volume) 
    {
        const char *methodName = "onSetVolumeAudioJniEngine";
        const char *signature  = "(IF)V"; // void (int,float)
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI, volume);
        return true;
    }
    
    bool AUDIO::setPan(const float pan )
    {
        const char *methodName = "onSetPanAudioJniEngine";
        const char *signature  = "(IF)V"; // void (int,float)
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI, pan);
        return true;
    }
    
    bool AUDIO::pause() 
    {
        if(isPlaying())
        {
            const char *methodName = "onPauseAudioJniEngine";
            const char *signature  = "(I)V"; // void (int)
            util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
            JNIEnv *         jenv = cJni->jenv;
            jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
            if (mid == nullptr)
                return log("method not found:", methodName, __LINE__);
            jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
            state = AUDIO_PLAYING;
            return true;
        }
        return false;
    }
    
    bool AUDIO::resume() 
    {
        if(state == AUDIO_PLAYING)
        {
            const char *     methodName = "onResumeAudioJniEngine";
            const char *     signature  = "(I)V"; // void (int)
            util::COMMON_JNI *cJni       = util::COMMON_JNI::getInstance();
            JNIEnv *         jenv       = cJni->jenv;
            jmethodID        mid        = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
            if (mid == nullptr)
                return log("method not found:", methodName, __LINE__);
            jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
            return true;
        }            
        return false;
    }
    
    bool AUDIO::stop()
    {
        const char *methodName = "onStopAudioJniEngine";
        const char *signature  = "(I)V"; // void (int)
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
        state = AUDIO_STOPPED;
        return true;
    }
    
    bool AUDIO::setPitch(const float pitch) // range from 0.5 to 2.0.  default is 1.0. - ok somente sound
    {
        const char *methodName = "onSetPitchAudioJniEngine";
        const char *signature  = "(IF)V"; // void (int,float)
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI, pitch);
        return true;
    }
    
    bool AUDIO::load(const char *filenameSound, const bool loop , const bool inMemory ) // mesmo que load - ok
    {
        if (filenameSound == nullptr)
            return false;
		if(isLoaded())
			return true;
        const char *methodName = "onLoadAudioJniEngine";
        const char *signature  = "(ILjava/lang/String;ZZ)Z"; // bool (int,string, bool, bool)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jstring jstr = jenv->NewStringUTF(cJni->get_safe_string_utf(filenameSound));//fixed issue using local std::string
        if (jstr == nullptr)
            return log("error on call ", "NewStringUTF", __LINE__);
        jboolean result =
            jenv->CallStaticBooleanMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI, jstr, loop, inMemory);
        jenv->DeleteLocalRef(jstr);
		fileName = util::getBaseName(filenameSound);
        return result;
    }
    
    bool AUDIO::play(const bool loop )
    {
        const char *methodName = "onPlayAudioJniEngine";
        const char *signature  = "(IZ)Z"; // bool (int,bool)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jboolean result = jenv->CallStaticBooleanMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI, loop);
        state = AUDIO_PLAYING;
        return result;
    }
    
    bool AUDIO::isPlaying()
    {
        if(state == AUDIO_STOPPED)
            return false;
        const char *methodName = "onIsPlayingAudioJniEngine";
        const char *signature  = "(I)Z"; // bool (int)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jboolean result = jenv->CallStaticBooleanMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
        if(result)
            state = AUDIO_PLAYING;
        else
            state = AUDIO_STOPPED;
        return state == AUDIO_PLAYING;
    }
    
    bool AUDIO::isPaused()
    {
        const char *methodName = "onIsPausedAudioJniEngine";
        const char *signature  = "(I)Z"; // bool (int)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        const jboolean result = jenv->CallStaticBooleanMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
        return result;
    }
    
    float AUDIO::getVolume()
    {
        const char *methodName = "onGetVolumeAudioJniEngine";
        const char *signature  = "(I)F"; // float (int)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        const jfloat result = jenv->CallStaticFloatMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
        return result;
    }
    
    float AUDIO::getPan()
    {
        const char *methodName = "onGetPanAudioJniEngine";
        const char *signature  = "(I)F"; // float (int)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        const jfloat result = jenv->CallStaticFloatMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
        return result;
    }
    
    float AUDIO::getPitch()
    {
        const char *methodName = "onGetPitchAudioJniEngine";
        const char *signature  = "(I)F"; // float (int)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        const jfloat result = jenv->CallStaticFloatMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
        return result;
    }
    
    int AUDIO::getLength()
    {
        const char *methodName = "onGetLengthAudioJniEngine";
        const char *signature  = "(I)I"; // int (int)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        const jint result = jenv->CallStaticIntMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
        return result;
    }
    
    bool AUDIO::reset()
    {
        const char *methodName = "onResetAudioJniEngine";
        const char *signature  = "(I)V"; // void (int)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI);
        return true;
    }

    bool AUDIO::setPosition(const int pos)
    {
        const char *methodName = "onSetPositionAudioJniEngine";
        const char *signature  = "(II)V"; // void (int,int)

        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (mid == nullptr)
            return log("method not found:", methodName, __LINE__);
        jenv->CallStaticVoidMethod(cJni->jclassAudioManagerJniEngine, mid, this->indexJNI, pos);
        return true;
    }

	bool AUDIO::isLoaded()
	{
		return fileName.size() > 0;
	}
    
    void AUDIO::setOnEndstream(AUDIO::OnEndStreamCallBack ptrOnEndStreamCallBack)
    {
        onEndStreamCallBack = ptrOnEndStreamCallBack;
    }
    
    AUDIO::OnEndStreamCallBack AUDIO::getOnEndstream() const
    {
        return onEndStreamCallBack;
    }
    
    bool AUDIO::log(const char *erro, const char *arg1, const int line)
    {
        ERROR_LOG("%s %s \nFile:[%s] line:[%d]", erro, arg1, __FILE__, line);
        return false;
    }
    
    bool AUDIO::onNew_AudioJniEngine(int *retIndexJni)
    {
        const char *     methodName           = "onNewAudioJniEngine";
        const char *     signature            = "()I";
        util::COMMON_JNI *cJni                = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv                 = cJni->jenv;
        jmethodID        midNew_AudioJniEngine = jenv->GetStaticMethodID(cJni->jclassAudioManagerJniEngine, methodName, signature);
        if (midNew_AudioJniEngine == nullptr)
            return log("method not found:", methodName, __LINE__);
        jint result  = jenv->CallStaticIntMethod(cJni->jclassAudioManagerJniEngine, midNew_AudioJniEngine);
        *retIndexJni = result;
        return true;
    }

    const char* AUDIO::getFileName() const noexcept
    {
        return this->fileName.c_str();
    }

    const char* AUDIO_ENGINE_version()
	{
		return "ANDROIND JNI";
	}
}
