-- From engine root: mini-mbm --scene src/test-lib/fluid_depth_smoke.lua --disable_select_monitor
-- Optional: MBM_FLUID_SHIFT=150 (same view, different sorting origin),
-- MBM_FLUID_MODE=2dw|2ds|3d (default 3d), MBM_FLUID_CAPTURE=/tmp/fluid.png (render-to-texture).
require 'box2dLiquidFun'
local objects, fluid, world, target
local elapsed = 0
function onInitScene()
    mbm.onErrorStop(true)
    -- Generated alpha texture keeps this regression independent of external game assets.
    local pixels={}
    for y=0,31 do for x=0,31 do
        local inside=(x-15.5)^2+(y-15.5)^2 < 13^2
        for _,v in ipairs({255,180,0,inside and 255 or 0}) do pixels[#pixels+1]=v end
    end end
    local fluidTexture=assert(mbm.createTexture(pixels,32,32,4,'fluid_depth_test'))
    mbm.setColor(0.12,0.12,0.12)
    mbm.setLightEnabled('3d',false)
    local shift = tonumber(os.getenv('MBM_FLUID_SHIFT')) or 0
    local mode = os.getenv('MBM_FLUID_MODE') or '3d'
    local cam=mbm.getCamera('3d')
    cam:setPos(shift,0,200);cam:setFocus(shift,0,0)
    world=box2dLiquidFun:new()
    world:setGravity(0,0)
    objects={}
    for _,info in ipairs({{-10,5,0.2,0.8,0.3},{10,-5,0.2,0.3,0.8}}) do
        local s=shape:new(mode,shift+info[1],0,info[2])
        assert(s:create('rectangle',20,40))
        assert(s:setColor(info[3],info[4],info[5],1))
        objects[#objects+1]=s
    end
    fluid=world:createFluid({type='circle',ray=8},{is3d=mode=='3d',is2dScreen=mode=='2ds',
        radius=.4,radiusScale=2,position={x=shift,y=0,z=0},texture=fluidTexture,blend='inv_src_alpha'})
    fluid:setPaused(true)
    assert(fluid:getParticleCount()>0)
    if os.getenv('MBM_FLUID_CAPTURE') then
        target=render2texture:new('2dw')
        assert(target:create(400,300,true))
        for _,s in ipairs(objects) do target:add(s) end
        target:add(fluid)
        local c=target:getCamera('3d');c:setPos(shift,0,200);c:setFocus(shift,0,0)
    end
end
function onLoop(delta)
    elapsed=elapsed+delta
    if elapsed>1 then
        if target then
            local path=os.getenv('MBM_FLUID_CAPTURE')
            assert(target:save(path))
            if (os.getenv('MBM_FLUID_MODE') or '3d') == '3d' then
                local pixels,w,h=mbm.readImagePixels(path)
                local exposed,occluded=0,0
                for y=0,h-1 do for x=0,w-1 do
                    local r,g,b=pixels:byte((y*w+x)*4+1,(y*w+x)*4+3)
                    if r>200 and g>100 and g<220 and b<40 then
                        if x<w/2 then exposed=exposed+1 else occluded=occluded+1 end
                    end
                end end
                assert(exposed>0, 'Fluid behind rear geometry or missing')
                assert(occluded==0, 'Fluid drawn over foreground geometry')
            end
        end
        print('FLUID_DEPTH_SMOKE_OK',fluid:getParticleCount())
        mbm.quit()
    end
end
