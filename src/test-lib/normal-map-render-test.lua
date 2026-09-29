--[[
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
]]

-- Run with --disable_select_monitor; require the PASS sentinel, not just exit code zero.
-- mkdir -p /tmp/mini-mbm-normal-render before running; inspect the PASS sentinel.
local folder=(os.getenv('MBM_NORMAL_MAP_RENDER_DIR') or '/tmp/mini-mbm-normal-render')..'/'
local nonIndexed=os.getenv('MBM_NORMAL_MAP_TEST_VB')=='1'
local object,rt
local step,ticks=1,0
local images={}
local cases={
    {'baseline','',1,'+Y'},
    {'neutral','#8080FFFF',1,'+Y'},
    {'detail','#FF8080FF',1,'+Y'},
    {'zero','#FF8080FF',0,'+Y'},
    {'green-plus','#80FFDAFF',1,'+Y'},
    {'green-minus','#8000DAFF',1,'-Y'},
    {'strong','#80FFDAFF',3,'+Y'},
    {'extreme','#80FFDAFF',1e30,'+Y'},
    {'removed','',1,'+Y'},
    {'scaled-baseline','',1,'+Y',nil,false,true},
    {'scaled-detail','#FF8080FF',1,'+Y',nil,false,true},
    {'mirror-baseline','',1,'+Y',nil,false,false,nil,true},
    {'mirror-detail','#FF8080FF',1,'+Y',nil,false,false,nil,true},
    {'mirror-neutral','#8080FFFF',1,'+Y',nil,false,false,nil,true},
    {'point-baseline','',1,'+Y',nil,true},
    {'point-detail','#FF8080FF',1,'+Y',nil,true},
    {'reserved-baseline','',1,'+Y','lit textured.ps'},
    {'reserved-detail','#FF8080FF',1,'+Y','lit textured.ps'},
    {'legacy-baseline','',1,'+Y','lit textured.ps',false,false,'legacy-normal.vs'},
    {'legacy-detail','#FF8080FF',1,'+Y','lit textured.ps',false,false,'legacy-normal.vs'},
    {'hud-baseline','',1,'+Y',nil,false,false,nil,false,'2ds'},
    {'hud-detail','#FF8080FF',1,'+Y',nil,false,false,nil,false,'2ds'},
    {'2dw-baseline','',1,'+Y',nil,false,false,nil,false,'2dw'},
    {'2dw-detail','#FF8080FF',1,'+Y',nil,false,false,nil,false,'2dw'},
}
local world='3d'
local retained={}
local function apply()
    local c=cases[step]
    if c[10] and c[10]~=world then
        world=c[10];retained[#retained+1]=object
        mbm.setLightEnabled('2dw',true);mbm.setAmbientLight('2dw',0.05,0.05,0.05)
        mbm.clearPointLights('2dw');mbm.addPointLight('2dw',20,10,20,100,0.3,0.3,0.3)
        object=mesh:new(world);assert(object:load(folder..'plane.msh'))
        rt:clear();rt:add(object)
    end
    object.sx=c[7] and 1.5 or 1;object.sy=c[7] and 0.7 or 1;object.ay=c[7] and 0.4 or 0
    if c[9] then object.sx=-1 end
    if world~='3d' then object.sx=30;object.sy=30 end
    assert(object:setMaterialTexture('normal',c[2]))
    assert(object:setNormalMapSettings(c[4],c[3]))
    if c[6] then
        mbm.setDirectionalLightColor('3d',0,0,0)
        mbm.clearPointLights('3d');mbm.addPointLight('3d',3,1,-3,20,0.3,0.3,0.3)
    else
        mbm.clearPointLights('3d');mbm.setDirectionalLightColor('3d',0.25,0.25,0.25)
    end
    if c[5] then assert(object:getShader():load(c[5],c[8])) end
end
function onInitScene()
    local ok,err=pcall(function()
        local legacyCode = [[
            attribute vec4 aPosition; attribute vec3 aNormal; attribute vec2 aTextCoord;
            uniform mat4 mvpMatrix; uniform mat4 mvMatrix;
            varying vec3 vNormalView; varying vec3 vPositionView; varying vec2 vTexCoord;
            void main() { gl_Position=mvpMatrix*aPosition; vNormalView=mat3(mvMatrix)*aNormal;
                vPositionView=(mvMatrix*aPosition).xyz; vTexCoord=aTextCoord; }
        ]]
        if mbm.get('USE_DIRECTX11') or mbm.get('USE_DIRECTX9') then
            legacyCode = [[
                cbuffer Matrices : register(b0) { row_major float4x4 mvpMatrix; row_major float4x4 mvMatrix; };
                struct Input { float4 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; };
                struct Output { float4 position:SV_POSITION; float3 normal:TEXCOORD1; float3 view:TEXCOORD2; float2 uv:TEXCOORD0; };
                Output main(Input input) { Output o; o.position=mul(input.position,mvpMatrix);
                    o.normal=mul(float4(input.normal,0),mvMatrix).xyz;
                    o.view=mul(input.position,mvMatrix).xyz; o.uv=input.uv; return o; }
            ]]
            if mbm.get('USE_DIRECTX9') then
                legacyCode = legacyCode:gsub('cbuffer Matrices : register%(b0%) { row_major float4x4 mvpMatrix; row_major float4x4 mvMatrix; };',
                    'float4x4 mvpMatrix; float4x4 mvMatrix;'):gsub('SV_POSITION','POSITION')
            end
        end
        assert(mbm.addShader({name='legacy-normal.vs',code=legacyCode}))
        mbm.setLightEnabled('3d',true)
        mbm.setAmbientLight('3d',0.05,0.05,0.05)
        mbm.setDirectionalLight('3d',-0.8,-0.3,0.6,0.25,0.25,0.25)
        local a=meshDebug:new();a:setType('mesh');a:setModeFrontFace('CCW');a:enableNormal(true);a:enableUv(true)
        local f=a:addFrame(3);local s=a:addSubSet(f)
        local function addQuad(subset,left,right)
            local vertices={
                {x=left,y=-1,z=0,nx=0,ny=0,nz=-1,u=0,v=0},
                {x=right,y=-1,z=0,nx=0,ny=0,nz=-1,u=1,v=0},
                {x=left,y=1,z=0,nx=0,ny=0,nz=-1,u=0,v=1},
                {x=right,y=1,z=0,nx=0,ny=0,nz=-1,u=1,v=1}}
            local indices={1,2,3,3,2,4,3,2,1,4,2,3} -- Both windings let the mirrored-scale case remain visible.
            if nonIndexed then
                local expanded={};for _,i in ipairs(indices) do expanded[#expanded+1]=vertices[i] end
                assert(a:addVertex(f,subset,expanded))
            else
                assert(a:addVertex(f,subset,vertices));assert(a:addIndex(f,subset,indices))
            end
        end
        addQuad(s,-1,0)
        local second=a:addSubSet(f);addQuad(second,0,1)
        assert(a:setTexture(f,second,'#FFFFFFFF'))
        a:setTexture(f,s,'#FFFFFFFF')
        a:setMaterialTexture(f,s,'normal','#8080FFFF')
        assert(a:addAnim('Static',1,1,1,0))
        assert(a:save(folder..'plane.msh',false,false,true))
        object=mesh:new('3d');assert(object:load(folder..'plane.msh'))
        rt=render2texture:new('2ds');assert(rt:create(96,96,true))
        rt:getCamera('3d'):setPos(0,0,-4);rt:getCamera('3d'):setFocus(0,0,0)
        mbm.getCamera('3d'):setPos(0,0,-4);mbm.getCamera('3d'):setFocus(0,0,0)
        rt:setColor(0,0,0,1)
        rt:add(object)
        apply()
    end)
    if not ok then print('NORMAL MAP VISUAL FAIL '..tostring(err));mbm.quit() end
end
local function difference(a,b)
    local sum=0
    for i=1,#a do sum=sum+math.abs(a:byte(i)-b:byte(i)) end
    return sum/#a
end
function onLoop()
    if not rt then return end
    ticks=ticks+1
    if ticks<4 then return end
    ticks=0
    local ok,err=pcall(function()
        local name=cases[step][1];local path=folder..name..'.png'
        assert(rt:save(path))
        images[name]=assert(mbm.readImagePixels(path))
        step=step+1
        if step<=#cases then apply();return end
        local base=images.baseline
        local neutral=difference(base,images.neutral)
        local detail=difference(base,images.detail)
        for y=0,95 do for x=49,95 do
            local at=(y*96+x)*4+1
            assert(base:sub(at,at+3)==images.detail:sub(at,at+3),'normal-map state leaked to subset without a map')
        end end
        local zero=difference(base,images.zero)
        local green=difference(images['green-plus'],images['green-minus'])
        assert(difference(images.strong,images['green-plus'])>1,'strength above one changes detail')
        assert(difference(images.extreme,base)>1,'finite extreme strength remains visible')
        local removed=difference(base,images.removed)
        assert(difference(images['legacy-baseline'],images['legacy-detail'])==0,'legacy vertex shader contract preserved')
        assert(difference(images['mirror-baseline'],images['mirror-neutral'])<1,'mirrored neutral equivalence')
        assert(difference(images['mirror-baseline'],images['mirror-detail'])>1,'mirrored detail remains visible')
        assert(difference(images['hud-baseline'],images['hud-detail'])==0,'HUD remains unlit')
        local flat=difference(images['2dw-baseline'],images['2dw-detail'])
        assert(flat>1,'2dw retains its existing normal-map lighting')
        print('NORMAL MAP 2DW DIFFERENCE '..flat)
        local scaled=difference(images['scaled-baseline'],images['scaled-detail'])
        assert(scaled>1,'rotated non-uniform scaling retains mapped normal')
        print('NORMAL MAP SCALED DIFFERENCE '..scaled)
        local point=difference(images['point-baseline'],images['point-detail'])
        assert(point>2,'point lighting must use mapped normal')
        print('NORMAL MAP POINT DIFFERENCE '..point)
        local reserved=difference(images['reserved-baseline'],images['reserved-detail'])
        print(string.format('NORMAL MAP METRICS neutral=%.4f detail=%.4f zero=%.4f green=%.4f removed=%.4f reserved=%.4f',neutral,detail,zero,green,removed,reserved))
        assert(neutral<1 and detail>1 and zero==0 and green<1 and removed==0 and reserved>1,'image comparisons')
        print('NORMAL MAP VISUAL PASS');mbm.quit()
    end)
    if not ok then print('NORMAL MAP VISUAL FAIL '..tostring(err));mbm.quit() end
end
