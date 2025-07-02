/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

#ifndef LAUNCHER2D_LUA_H
#define LAUNCHER2D_LUA_H

static const char * p_launcherLua = 
R"LUA_SCENE(
--[[
  This is the default scene launcher for the engine.
  It uses coroutine to partially load each mesh.
  It implements the camera if it is not implemented.
  It abstracts the function when the scene is of the 'class table' style.
--]]


function __onLoadScene(fileName)
    local __old_onInitScene = onInitScene
    local __old_loop        = loop
    local __fullFileName    = mbm.getFullPath(fileName)
    local __scene, __err    = dofile(__fullFileName)
    if __scene then
        __t_my_class = __scene
        if type(__scene.load) == 'function' then --from editor? we load the meshes.
            __scene:load(onProgress or function(percent) print('loading:',percent) end)
        end
        if __scene and type(__scene.onInitScene) == 'function' then
            __scene:onInitScene()
        end
    else
        if type(onInitScene) == 'function' and onInitScene ~= __old_onInitScene then
            onInitScene()
            if type(loop) == 'function' and loop ~= __old_loop then
                cCoroutineLoadScene = nil
            end
        elseif __err then
            print('error', tostring(__err))
        end
    end
end

function onInitScene()
    mbm.onErrorStop(true)
    bEnableMoveCam2d = true
    camera2d    = mbm.getCamera('2d')
    camera2d.mx = 0
    camera2d.my = 0
    
    if mbm.getGlobal('disable-coroutine-loading') then
        fileNameScene = mbm.getGlobal('fileNameScene')
        if fileNameScene then
            __onLoadScene(fileNameScene)
        end
    else
        fileNameScene = mbm.getGlobal('fileNameScene')
        if fileNameScene then
            cCoroutineLoadScene = coroutine.create(__onLoadScene)
        end
    end
end

function loop(delta)
    if cCoroutineLoadScene then
        
        local __status = coroutine.status (cCoroutineLoadScene)
        if __status == 'suspended' or  __status == 'normal' then
            local __ret, __i_current, __i_total = coroutine.resume(cCoroutineLoadScene,fileNameScene)
            if  __ret == false then--error
                print('error',__i_current)
            else
                if type(__i_current) == 'number' and (type(__i_total) == 'number' or __assuming_1_to_100_cc_loading) then
                    if type(__i_total) ~= 'number' and __assuming_1_to_100_cc_loading then
                        __i_total = math.max(100,__i_current)
                    end
                    __i_total = __i_total + 1 --avoid call 100% twice
                    if __t_my_class and type(__t_my_class.onProgress) == 'function' then
                        __t_my_class:onProgress(__i_current / __i_total * 100)
                    elseif type(onProgress) == 'function' then
                        onProgress(__i_current / __i_total * 100)
                    end
                elseif type(__i_current) == 'number' then
                    print('error','coroutine.yield has been called passing one argument, it should be two (iStep,iTotal)\nassuming 1 to 100...')
                    __assuming_1_to_100_cc_loading = true
                elseif __i_current then
                    print('error','coroutine.yield has been called passing not number as argument, it should be two (iStep,iTotal)')
                end
            end
        elseif __status == 'dead' then -- end of routine
            cCoroutineLoadScene = nil
            local tSplash = mbm.getSplash()
            if tSplash then
                tSplash.visible = false
            end
            mbm.setFakeFps(120,60)
            if __t_my_class and type(__t_my_class.onProgress) == 'function' then
                __t_my_class:onProgress(100)
            elseif type(onProgress) == 'function' then
                onProgress(100)
            end
        end
    elseif __t_my_class and type(__t_my_class.loop) == 'function' then
        __t_my_class:loop(delta)
    end
end

function onTouchMove(key,x,y)
    if cCoroutineLoadScene then return end
    if bEnableMoveCam2d and isClickedMouseleft then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px,camera2d.y - py)
    end
    if __t_my_class and type(__t_my_class.onTouchMove) == 'function' then
        __t_my_class:onTouchMove(key,x,y)
    end
end

function onTouchDown(key,x,y)
    if cCoroutineLoadScene then return end
    isClickedMouseleft = true
    camera2d.mx = x
    camera2d.my = y
    if __t_my_class and type(__t_my_class.onTouchDown) == 'function' then
        __t_my_class:onTouchDown(key,x,y)
    end
end

function onTouchUp(key,x,y)
    if cCoroutineLoadScene then return end
    isClickedMouseleft = false
    camera2d.mx = x
    camera2d.my = y
    if __t_my_class and type(__t_my_class.onTouchUp) == 'function' then
        __t_my_class:onTouchUp(key,x,y)
    end
end

function onTouchZoom(zoom)
    if cCoroutineLoadScene then return end
    if __t_my_class and type(__t_my_class.onTouchZoom) == 'function' then
        __t_my_class:onTouchZoom(zoom)
    end
end

function onKeyDown(key)
    if cCoroutineLoadScene then return end
    if key == mbm.getKeyCode('ESC') then
        mbm.quit()
    end
    if __t_my_class and type(__t_my_class.onKeyDown) == 'function' then
        __t_my_class:onKeyDown(key)
    end
end

function onKeyUp(key)
    if cCoroutineLoadScene then return end
    if key == mbm.getKeyCode('F5') then--force refresh (test)
        mbm.refresh()
    end
    if __t_my_class and type(__t_my_class.onKeyUp) == 'function' then
        __t_my_class:onKeyUp(key)
    end
end

function onEndScene()
    if __t_my_class and type(__t_my_class.onEndScene) == 'function' then
        __t_my_class:onEndScene()
    end
end

function onRestore()
    if __t_my_class and type(__t_my_class.onRestore) == 'function' then
        __t_my_class:onRestore()
    end
end

function onInfoJoystick(player,maxButtons,deviceName,extra)
    if cCoroutineLoadScene then return end
    if __t_my_class and type(__t_my_class.onInfoJoystick) == 'function' then
        __t_my_class:onInfoJoystick(player,maxButtons,deviceName,extra)
    end
end

function onKeyDownJoystick(player,key)
    if cCoroutineLoadScene then return end
    if __t_my_class and type(__t_my_class.onKeyDownJoystick) == 'function' then
        __t_my_class:onKeyDownJoystick(player,key)
    end
end

function onKeyUpJoystick(player,key)
    if cCoroutineLoadScene then return end
    if __t_my_class and type(__t_my_class.onKeyUpJoystick) == 'function' then
        __t_my_class:onKeyUpJoystick(player,key)
    end
end

function onMoveJoystick(player,lx,ly,rx,ry)
    if cCoroutineLoadScene then return end
    if __t_my_class and type(__t_my_class.onMoveJoystick) == 'function' then
        __t_my_class:onMoveJoystick(player,lx,ly,rx,ry)
    end
end

)LUA_SCENE";

#endif