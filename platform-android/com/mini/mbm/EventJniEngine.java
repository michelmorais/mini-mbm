package com.mini.mbm;

enum EventJniEngineName
{
	onTouchDown,
	onTouchUp,
	onTouchMove,
	onTouchZoom,
	onKeyDown,
	onKeyUp,
	onKeyDownJoystick,
	onKeyUpJoystick,
	onInfoDeviceJoystick,
	onMoveJoystick,
	onStreamStopped,
	onCallBackCommands,
}

public class EventJniEngine
{
	public final float x;
	public final float y;
	public final int   key;
	public EventJniEngineName eventType;
	public final String param;
	public final String param2;

	public EventJniEngine(final float x,final float y,final int key,final EventJniEngineName eventName,String param,String param2)
	{
		this.x = x;
		this.y = y;
		this.key = key;
		this.eventType = eventName;
		this.param = param;
		this.param2 = param2;
	}

}
