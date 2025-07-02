package com.mini.mbm;

import android.annotation.SuppressLint;
import android.os.Build;
import android.util.SparseArray;
import android.view.KeyEvent;

public class KeyCodeJniEngine 
{
	@SuppressLint("NewApi")
	public static void buildMapKey(SparseArray<String> keyNames)
	{
		keyNames.clear();
		keyNames.append(KeyEvent.KEYCODE_UNKNOWN, "KEYCODE_UNKNOWN");
		keyNames.append(KeyEvent.KEYCODE_SOFT_LEFT, "KEYCODE_SOFT_LEFT");
		keyNames.append(KeyEvent.KEYCODE_SOFT_RIGHT, "KEYCODE_SOFT_RIGHT");
		keyNames.append(KeyEvent.KEYCODE_HOME, "KEYCODE_HOME");
		keyNames.append(KeyEvent.KEYCODE_BACK, "KEYCODE_BACK");
		keyNames.append(KeyEvent.KEYCODE_BACK, "ESCAPE");
		keyNames.append(KeyEvent.KEYCODE_BACK, "ESC");
		keyNames.append(KeyEvent.KEYCODE_CALL, "KEYCODE_CALL");
		keyNames.append(KeyEvent.KEYCODE_ENDCALL, "KEYCODE_ENDCALL");
		keyNames.append(KeyEvent.KEYCODE_0, "KEYCODE_0");
		keyNames.append(KeyEvent.KEYCODE_1, "KEYCODE_1");
		keyNames.append(KeyEvent.KEYCODE_2, "KEYCODE_2");
		keyNames.append(KeyEvent.KEYCODE_3, "KEYCODE_3");
		keyNames.append(KeyEvent.KEYCODE_4, "KEYCODE_4");
		keyNames.append(KeyEvent.KEYCODE_5, "KEYCODE_5");
		keyNames.append(KeyEvent.KEYCODE_6, "KEYCODE_6");
		keyNames.append(KeyEvent.KEYCODE_7, "KEYCODE_7");
		keyNames.append(KeyEvent.KEYCODE_8, "KEYCODE_8");
		keyNames.append(KeyEvent.KEYCODE_9, "KEYCODE_9");
		keyNames.append(KeyEvent.KEYCODE_STAR, "KEYCODE_STAR");
		keyNames.append(KeyEvent.KEYCODE_POUND, "KEYCODE_POUND");
		keyNames.append(KeyEvent.KEYCODE_DPAD_UP, "KEYCODE_DPAD_UP");
		keyNames.append(KeyEvent.KEYCODE_DPAD_DOWN, "KEYCODE_DPAD_DOWN");
		keyNames.append(KeyEvent.KEYCODE_DPAD_LEFT, "KEYCODE_DPAD_LEFT");
		keyNames.append(KeyEvent.KEYCODE_DPAD_RIGHT, "KEYCODE_DPAD_RIGHT");
		keyNames.append(KeyEvent.KEYCODE_DPAD_CENTER, "KEYCODE_DPAD_CENTER");
		keyNames.append(KeyEvent.KEYCODE_VOLUME_UP, "KEYCODE_VOLUME_UP");
		keyNames.append(KeyEvent.KEYCODE_VOLUME_DOWN, "KEYCODE_VOLUME_DOWN");
		keyNames.append(KeyEvent.KEYCODE_POWER, "KEYCODE_POWER");
		keyNames.append(KeyEvent.KEYCODE_CAMERA, "KEYCODE_CAMERA");
		keyNames.append(KeyEvent.KEYCODE_CLEAR, "KEYCODE_CLEAR");
		keyNames.append(KeyEvent.KEYCODE_A, "KEYCODE_A");
		keyNames.append(KeyEvent.KEYCODE_B, "KEYCODE_B");
		keyNames.append(KeyEvent.KEYCODE_C, "KEYCODE_C");
		keyNames.append(KeyEvent.KEYCODE_D, "KEYCODE_D");
		keyNames.append(KeyEvent.KEYCODE_E, "KEYCODE_E");
		keyNames.append(KeyEvent.KEYCODE_F, "KEYCODE_F");
		keyNames.append(KeyEvent.KEYCODE_G, "KEYCODE_G");
		keyNames.append(KeyEvent.KEYCODE_H, "KEYCODE_H");
		keyNames.append(KeyEvent.KEYCODE_I, "KEYCODE_I");
		keyNames.append(KeyEvent.KEYCODE_J, "KEYCODE_J");
		keyNames.append(KeyEvent.KEYCODE_K, "KEYCODE_K");
		keyNames.append(KeyEvent.KEYCODE_L, "KEYCODE_L");
		keyNames.append(KeyEvent.KEYCODE_M, "KEYCODE_M");
		keyNames.append(KeyEvent.KEYCODE_N, "KEYCODE_N");
		keyNames.append(KeyEvent.KEYCODE_O, "KEYCODE_O");
		keyNames.append(KeyEvent.KEYCODE_P, "KEYCODE_P");
		keyNames.append(KeyEvent.KEYCODE_Q, "KEYCODE_Q");
		keyNames.append(KeyEvent.KEYCODE_R, "KEYCODE_R");
		keyNames.append(KeyEvent.KEYCODE_S, "KEYCODE_S");
		keyNames.append(KeyEvent.KEYCODE_T, "KEYCODE_T");
		keyNames.append(KeyEvent.KEYCODE_U, "KEYCODE_U");
		keyNames.append(KeyEvent.KEYCODE_V, "KEYCODE_V");
		keyNames.append(KeyEvent.KEYCODE_W, "KEYCODE_W");
		keyNames.append(KeyEvent.KEYCODE_X, "KEYCODE_X");
		keyNames.append(KeyEvent.KEYCODE_Y, "KEYCODE_Y");
		keyNames.append(KeyEvent.KEYCODE_Z, "KEYCODE_Z");
		keyNames.append(KeyEvent.KEYCODE_COMMA, "KEYCODE_COMMA");
		keyNames.append(KeyEvent.KEYCODE_PERIOD, "KEYCODE_PERIOD");
		keyNames.append(KeyEvent.KEYCODE_ALT_LEFT, "KEYCODE_ALT_LEFT");
		keyNames.append(KeyEvent.KEYCODE_ALT_RIGHT, "KEYCODE_ALT_RIGHT");
		keyNames.append(KeyEvent.KEYCODE_SHIFT_LEFT, "KEYCODE_SHIFT_LEFT");
		keyNames.append(KeyEvent.KEYCODE_SHIFT_RIGHT, "KEYCODE_SHIFT_RIGHT");
		keyNames.append(KeyEvent.KEYCODE_TAB, "KEYCODE_TAB");
		keyNames.append(KeyEvent.KEYCODE_SPACE, "KEYCODE_SPACE");
		keyNames.append(KeyEvent.KEYCODE_SYM, "KEYCODE_SYM");
		keyNames.append(KeyEvent.KEYCODE_EXPLORER, "KEYCODE_EXPLORER");
		keyNames.append(KeyEvent.KEYCODE_ENVELOPE, "KEYCODE_ENVELOPE");
		keyNames.append(KeyEvent.KEYCODE_ENTER, "KEYCODE_ENTER");
		keyNames.append(KeyEvent.KEYCODE_DEL, "KEYCODE_DEL");
		keyNames.append(KeyEvent.KEYCODE_GRAVE, "KEYCODE_GRAVE");
		keyNames.append(KeyEvent.KEYCODE_MINUS, "KEYCODE_MINUS");
		keyNames.append(KeyEvent.KEYCODE_EQUALS, "KEYCODE_EQUALS");
		keyNames.append(KeyEvent.KEYCODE_LEFT_BRACKET, "KEYCODE_LEFT_BRACKET");
		keyNames.append(KeyEvent.KEYCODE_RIGHT_BRACKET, "KEYCODE_RIGHT_BRACKET");
		keyNames.append(KeyEvent.KEYCODE_BACKSLASH, "KEYCODE_BACKSLASH");
		keyNames.append(KeyEvent.KEYCODE_SEMICOLON, "KEYCODE_SEMICOLON");
		keyNames.append(KeyEvent.KEYCODE_APOSTROPHE, "KEYCODE_APOSTROPHE");
		keyNames.append(KeyEvent.KEYCODE_SLASH, "KEYCODE_SLASH");
		keyNames.append(KeyEvent.KEYCODE_AT, "KEYCODE_AT");
		keyNames.append(KeyEvent.KEYCODE_NUM, "KEYCODE_NUM");
		keyNames.append(KeyEvent.KEYCODE_HEADSETHOOK, "KEYCODE_HEADSETHOOK");
		keyNames.append(KeyEvent.KEYCODE_FOCUS, "KEYCODE_FOCUS");
		keyNames.append(KeyEvent.KEYCODE_PLUS, "KEYCODE_PLUS");
		keyNames.append(KeyEvent.KEYCODE_MENU, "KEYCODE_MENU");
		keyNames.append(KeyEvent.KEYCODE_NOTIFICATION, "KEYCODE_NOTIFICATION");
		keyNames.append(KeyEvent.KEYCODE_SEARCH, "KEYCODE_SEARCH");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE, "KEYCODE_MEDIA_PLAY_PAUSE");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_STOP, "KEYCODE_MEDIA_STOP");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_NEXT, "KEYCODE_MEDIA_NEXT");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_PREVIOUS, "KEYCODE_MEDIA_PREVIOUS");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_REWIND, "KEYCODE_MEDIA_REWIND");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_FAST_FORWARD, "KEYCODE_MEDIA_FAST_FORWARD");
		keyNames.append(KeyEvent.KEYCODE_MUTE, "KEYCODE_MUTE");
		keyNames.append(KeyEvent.KEYCODE_PAGE_UP, "KEYCODE_PAGE_UP");
		keyNames.append(KeyEvent.KEYCODE_PAGE_DOWN, "KEYCODE_PAGE_DOWN");
		keyNames.append(KeyEvent.KEYCODE_PICTSYMBOLS, "KEYCODE_PICTSYMBOLS");
		keyNames.append(KeyEvent.KEYCODE_SWITCH_CHARSET, "KEYCODE_SWITCH_CHARSET");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_A, "KEYCODE_BUTTON_A");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_B, "KEYCODE_BUTTON_B");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_C, "KEYCODE_BUTTON_C");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_X, "KEYCODE_BUTTON_X");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_Y, "KEYCODE_BUTTON_Y");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_Z, "KEYCODE_BUTTON_Z");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_L1, "KEYCODE_BUTTON_L1");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_R1, "KEYCODE_BUTTON_R1");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_L2, "KEYCODE_BUTTON_L2");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_R2, "KEYCODE_BUTTON_R2");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_THUMBL, "KEYCODE_BUTTON_THUMBL");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_THUMBR, "KEYCODE_BUTTON_THUMBR");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_START, "KEYCODE_BUTTON_START");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_SELECT, "KEYCODE_BUTTON_SELECT");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_MODE, "KEYCODE_BUTTON_MODE");
		keyNames.append(KeyEvent.KEYCODE_ESCAPE, "KEYCODE_ESCAPE");
		keyNames.append(KeyEvent.KEYCODE_FORWARD_DEL, "KEYCODE_FORWARD_DEL");
		keyNames.append(KeyEvent.KEYCODE_CTRL_LEFT, "KEYCODE_CTRL_LEFT");
		keyNames.append(KeyEvent.KEYCODE_CTRL_RIGHT, "KEYCODE_CTRL_RIGHT");
		keyNames.append(KeyEvent.KEYCODE_CAPS_LOCK, "KEYCODE_CAPS_LOCK");
		keyNames.append(KeyEvent.KEYCODE_SCROLL_LOCK, "KEYCODE_SCROLL_LOCK");
		keyNames.append(KeyEvent.KEYCODE_META_LEFT, "KEYCODE_META_LEFT");
		keyNames.append(KeyEvent.KEYCODE_META_RIGHT, "KEYCODE_META_RIGHT");
		keyNames.append(KeyEvent.KEYCODE_FUNCTION, "KEYCODE_FUNCTION");
		keyNames.append(KeyEvent.KEYCODE_SYSRQ, "KEYCODE_SYSRQ");
		keyNames.append(KeyEvent.KEYCODE_BREAK, "KEYCODE_BREAK");
		keyNames.append(KeyEvent.KEYCODE_MOVE_HOME, "KEYCODE_MOVE_HOME");
		keyNames.append(KeyEvent.KEYCODE_MOVE_END, "KEYCODE_MOVE_END");
		keyNames.append(KeyEvent.KEYCODE_INSERT, "KEYCODE_INSERT");
		keyNames.append(KeyEvent.KEYCODE_FORWARD, "KEYCODE_FORWARD");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_PLAY, "KEYCODE_MEDIA_PLAY");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_PAUSE, "KEYCODE_MEDIA_PAUSE");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_CLOSE, "KEYCODE_MEDIA_CLOSE");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_EJECT, "KEYCODE_MEDIA_EJECT");
		keyNames.append(KeyEvent.KEYCODE_MEDIA_RECORD, "KEYCODE_MEDIA_RECORD");
		keyNames.append(KeyEvent.KEYCODE_F1, "KEYCODE_F1");
		keyNames.append(KeyEvent.KEYCODE_F2, "KEYCODE_F2");
		keyNames.append(KeyEvent.KEYCODE_F3, "KEYCODE_F3");
		keyNames.append(KeyEvent.KEYCODE_F4, "KEYCODE_F4");
		keyNames.append(KeyEvent.KEYCODE_F5, "KEYCODE_F5");
		keyNames.append(KeyEvent.KEYCODE_F6, "KEYCODE_F6");
		keyNames.append(KeyEvent.KEYCODE_F7, "KEYCODE_F7");
		keyNames.append(KeyEvent.KEYCODE_F8, "KEYCODE_F8");
		keyNames.append(KeyEvent.KEYCODE_F9, "KEYCODE_F9");
		keyNames.append(KeyEvent.KEYCODE_F10, "KEYCODE_F10");
		keyNames.append(KeyEvent.KEYCODE_F11, "KEYCODE_F11");
		keyNames.append(KeyEvent.KEYCODE_F12, "KEYCODE_F12");
		keyNames.append(KeyEvent.KEYCODE_NUM_LOCK, "KEYCODE_NUM_LOCK");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_0, "KEYCODE_NUMPAD_0");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_1, "KEYCODE_NUMPAD_1");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_2, "KEYCODE_NUMPAD_2");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_3, "KEYCODE_NUMPAD_3");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_4, "KEYCODE_NUMPAD_4");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_5, "KEYCODE_NUMPAD_5");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_6, "KEYCODE_NUMPAD_6");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_7, "KEYCODE_NUMPAD_7");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_8, "KEYCODE_NUMPAD_8");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_9, "KEYCODE_NUMPAD_9");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_DIVIDE, "KEYCODE_NUMPAD_DIVIDE");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_MULTIPLY, "KEYCODE_NUMPAD_MULTIPLY");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_SUBTRACT, "KEYCODE_NUMPAD_SUBTRACT");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_ADD, "KEYCODE_NUMPAD_ADD");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_DOT, "KEYCODE_NUMPAD_DOT");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_COMMA, "KEYCODE_NUMPAD_COMMA");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_ENTER, "KEYCODE_NUMPAD_ENTER");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_EQUALS, "KEYCODE_NUMPAD_EQUALS");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_LEFT_PAREN, "KEYCODE_NUMPAD_LEFT_PAREN");
		keyNames.append(KeyEvent.KEYCODE_NUMPAD_RIGHT_PAREN, "KEYCODE_NUMPAD_RIGHT_PAREN");
		keyNames.append(KeyEvent.KEYCODE_VOLUME_MUTE, "KEYCODE_VOLUME_MUTE");
		keyNames.append(KeyEvent.KEYCODE_INFO, "KEYCODE_INFO");
		keyNames.append(KeyEvent.KEYCODE_CHANNEL_UP, "KEYCODE_CHANNEL_UP");
		keyNames.append(KeyEvent.KEYCODE_CHANNEL_DOWN, "KEYCODE_CHANNEL_DOWN");
		keyNames.append(KeyEvent.KEYCODE_ZOOM_IN, "KEYCODE_ZOOM_IN");
		keyNames.append(KeyEvent.KEYCODE_ZOOM_OUT, "KEYCODE_ZOOM_OUT");
		keyNames.append(KeyEvent.KEYCODE_TV, "KEYCODE_TV");
		keyNames.append(KeyEvent.KEYCODE_WINDOW, "KEYCODE_WINDOW");
		keyNames.append(KeyEvent.KEYCODE_GUIDE, "KEYCODE_GUIDE");
		keyNames.append(KeyEvent.KEYCODE_DVR, "KEYCODE_DVR");
		keyNames.append(KeyEvent.KEYCODE_BOOKMARK, "KEYCODE_BOOKMARK");
		keyNames.append(KeyEvent.KEYCODE_CAPTIONS, "KEYCODE_CAPTIONS");
		keyNames.append(KeyEvent.KEYCODE_SETTINGS, "KEYCODE_SETTINGS");
		keyNames.append(KeyEvent.KEYCODE_TV_POWER, "KEYCODE_TV_POWER");
		keyNames.append(KeyEvent.KEYCODE_TV_INPUT, "KEYCODE_TV_INPUT");
		keyNames.append(KeyEvent.KEYCODE_STB_INPUT, "KEYCODE_STB_INPUT");
		keyNames.append(KeyEvent.KEYCODE_STB_POWER, "KEYCODE_STB_POWER");
		keyNames.append(KeyEvent.KEYCODE_AVR_POWER, "KEYCODE_AVR_POWER");
		keyNames.append(KeyEvent.KEYCODE_AVR_INPUT, "KEYCODE_AVR_INPUT");
		keyNames.append(KeyEvent.KEYCODE_PROG_RED, "KEYCODE_PROG_RED");
		keyNames.append(KeyEvent.KEYCODE_PROG_GREEN, "KEYCODE_PROG_GREEN");
		keyNames.append(KeyEvent.KEYCODE_PROG_YELLOW, "KEYCODE_PROG_YELLOW");
		keyNames.append(KeyEvent.KEYCODE_PROG_BLUE, "KEYCODE_PROG_BLUE");
		keyNames.append(KeyEvent.KEYCODE_APP_SWITCH, "KEYCODE_APP_SWITCH");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_1, "KEYCODE_BUTTON_1");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_2, "KEYCODE_BUTTON_2");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_3, "KEYCODE_BUTTON_3");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_4, "KEYCODE_BUTTON_4");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_5, "KEYCODE_BUTTON_5");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_6, "KEYCODE_BUTTON_6");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_7, "KEYCODE_BUTTON_7");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_8, "KEYCODE_BUTTON_8");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_9, "KEYCODE_BUTTON_9");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_10, "KEYCODE_BUTTON_10");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_11, "KEYCODE_BUTTON_11");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_12, "KEYCODE_BUTTON_12");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_13, "KEYCODE_BUTTON_13");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_14, "KEYCODE_BUTTON_14");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_15, "KEYCODE_BUTTON_15");
		keyNames.append(KeyEvent.KEYCODE_BUTTON_16, "KEYCODE_BUTTON_16");
		if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH)
		{
			keyNames.append(KeyEvent.KEYCODE_LANGUAGE_SWITCH, "KEYCODE_LANGUAGE_SWITCH");
			keyNames.append(KeyEvent.KEYCODE_MANNER_MODE, "KEYCODE_MANNER_MODE");
			keyNames.append(KeyEvent.KEYCODE_3D_MODE, "KEYCODE_3D_MODE");
		}
		if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH_MR1)
		{
			keyNames.append(KeyEvent.KEYCODE_CONTACTS, "KEYCODE_CONTACTS");
			keyNames.append(KeyEvent.KEYCODE_CALENDAR, "KEYCODE_CALENDAR");
			keyNames.append(KeyEvent.KEYCODE_MUSIC, "KEYCODE_MUSIC");
			keyNames.append(KeyEvent.KEYCODE_CALCULATOR, "KEYCODE_CALCULATOR");
		}
		if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN)
		{
			keyNames.append(KeyEvent.KEYCODE_ZENKAKU_HANKAKU, "KEYCODE_ZENKAKU_HANKAKU");
			keyNames.append(KeyEvent.KEYCODE_EISU, "KEYCODE_EISU");
			keyNames.append(KeyEvent.KEYCODE_MUHENKAN, "KEYCODE_MUHENKAN");
			keyNames.append(KeyEvent.KEYCODE_HENKAN, "KEYCODE_HENKAN");
			keyNames.append(KeyEvent.KEYCODE_KATAKANA_HIRAGANA, "KEYCODE_KATAKANA_HIRAGANA");
			keyNames.append(KeyEvent.KEYCODE_YEN, "KEYCODE_YEN");
			keyNames.append(KeyEvent.KEYCODE_RO, "KEYCODE_RO");
			keyNames.append(KeyEvent.KEYCODE_KANA, "KEYCODE_KANA");
			keyNames.append(KeyEvent.KEYCODE_ASSIST, "KEYCODE_ASSIST");
		}
		if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2)
		{
			keyNames.append(KeyEvent.KEYCODE_BRIGHTNESS_DOWN, "KEYCODE_BRIGHTNESS_DOWN");
			keyNames.append(KeyEvent.KEYCODE_BRIGHTNESS_UP, "KEYCODE_BRIGHTNESS_UP");
		}
		if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
		{
			keyNames.append(KeyEvent.KEYCODE_MEDIA_AUDIO_TRACK, "KEYCODE_MEDIA_AUDIO_TRACK");
		}
	}
	
	@SuppressLint("NewApi")
	@SuppressWarnings("unused")
	public static int getKeyCode(String key)
	{
		try
		{
			int strKey = KeyEvent.keyCodeFromString(key);
			if(strKey == 0)
			{
				if(key.equals("ESC"))
					return getKeyCode("KEYCODE_BACK");
				if(key.equals("ESCAPE"))
					return getKeyCode("KEYCODE_BACK");
				return 0;
			}
			return strKey;
		}
		catch(Exception e)
		{
			return 0;
		}
	}
	
	@SuppressLint("NewApi")
	@SuppressWarnings("unused")
	public static String getKeyName(int key)
	{
		try
		{
			String strKey = KeyEvent.keyCodeToString(key);
			if(strKey == null)
			{
				final int keyCode = getKeyCode("KEYCODE_BACK");
				if(keyCode == key)
					return ("KEYCODE_BACK");
			}
			return strKey;
		}
		catch(Exception e)
		{
			return null;
		}
	}

}
