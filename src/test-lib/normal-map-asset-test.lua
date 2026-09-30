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

-- Read-only render comparison of an external static mesh. Outputs go to an existing directory.
-- MBM_NORMAL_MAP_TEST_MESH=/path/to/module_001.msh MBM_NORMAL_MAP_RENDER_DIR=/tmp/output
local path=assert(os.getenv('MBM_NORMAL_MAP_TEST_MESH'))
local folder=assert(os.getenv('MBM_NORMAL_MAP_RENDER_DIR'))..'/'
local object,rt,author
local stage,ticks=1,0
local names={'asset-baseline','asset-detail','asset-neutral'}
local pixels={}
local settings={}
local function apply()
    for s,setting in ipairs(settings) do
        assert(object:setNormalMapSettings(setting[1],stage==1 and 0 or setting[2],s))
        if stage==3 and author:getMaterialTexture(1,s,'normal') then
            assert(object:setMaterialTexture('normal','#8080FFFF',true,s))
        end
    end
end
function onInitScene()
    local ok,err=pcall(function()
        mbm.addPath(path:match('^(.*)[/\\]'))
        mbm.setLightEnabled('3d',true)
        mbm.setAmbientLight('3d',0.08,0.08,0.08)
        mbm.setDirectionalLight('3d',-0.8,-0.3,0.6,0.55,0.55,0.55)
        author=meshDebug:new();assert(author:load(path))
        local low={math.huge,math.huge,math.huge}
        local high={-math.huge,-math.huge,-math.huge}
        for s=1,author:getTotalSubset(1) do
            settings[s]={author:getNormalMapSettings(1,s)}
            for _,v in ipairs(author:getVertex(1,s,1,author:getTotalVertex(1,s))) do
                for i,key in ipairs({'x','y','z'}) do low[i]=math.min(low[i],v[key]);high[i]=math.max(high[i],v[key]) end
            end
            print('ASSET NORMAL '..s..' '..tostring(author:getMaterialTexture(1,s,'normal')))
        end
        object=mesh:new('3d');assert(object:load(path))
        object:setPos(-(low[1]+high[1])/2,-(low[2]+high[2])/2,-(low[3]+high[3])/2)
        local extent=math.max(high[1]-low[1],high[2]-low[2],high[3]-low[3])
        assert(extent>0)
        rt=render2texture:new('2ds');assert(rt:create(512,512,true));rt:setColor(0.03,0.03,0.03,1)
        rt:getCamera('3d'):setPos(extent*0.8,extent*0.55,-extent*1.6)
        rt:getCamera('3d'):setFocus(0,0,0)
        rt:add(object)
        print('ASSET EXTENT '..extent)
        apply()
    end)
    if not ok then print('NORMAL MAP ASSET FAIL '..tostring(err));mbm.quit() end
end
local function difference(a,b)
    local sum=0
    for i=1,#a do sum=sum+math.abs(a:byte(i)-b:byte(i)) end
    return sum/#a
end
function onLoop()
    ticks=ticks+1
    if ticks<4 or not rt then return end
    ticks=0
    local ok,err=pcall(function()
        local output=folder..names[stage]..'.png'
        assert(rt:save(output));pixels[stage]=assert(mbm.readImagePixels(output))
        stage=stage+1
        if stage<=#names then apply();return end
        local detail=difference(pixels[1],pixels[2])
        local neutral=difference(pixels[1],pixels[3])
        print(string.format('NORMAL MAP ASSET METRICS detail=%.4f neutral=%.4f',detail,neutral))
        if mbm.isNormalMapping3DCompiled() then
            assert(detail>0.1 and neutral<1,'detail visible and neutral equivalent')
        else
            assert(detail==0 and neutral==0,'disabled 3D mapping retains geometric normals')
        end
        print('NORMAL MAP ASSET PASS');mbm.quit()
    end)
    if not ok then print('NORMAL MAP ASSET FAIL '..tostring(err));mbm.quit() end
end
