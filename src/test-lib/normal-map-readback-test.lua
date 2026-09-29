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

-- Generate fixtures with testLib --normal-map-persistence-tests using the same directory.
-- Run with --disable_select_monitor; require the PASS sentinel, not just exit code zero.
local dir=assert(os.getenv('MBM_NORMAL_MAP_FIXTURE_DIR'))..'/'
local objects={}
local remaining=0
local failed=false
local started=0
local function compare(a,b)
    assert(a:getTotalFrame()==b:getTotalFrame(),'frame count')
    for f=1,a:getTotalFrame() do
        assert(a:getTotalSubset(f)==b:getTotalSubset(f),'subset count')
        for s=1,a:getTotalSubset(f) do
            local count=a:getTotalVertex(f,s)
            assert(count==b:getTotalVertex(f,s),'source vertex count')
            local av,bv=a:getVertex(f,s,1,count),b:getVertex(f,s,1,count)
            for i,v in ipairs(av) do
                for _,key in ipairs({'x','y','z','nx','ny','nz','u','v'}) do
                    assert(v[key]==bv[i][key],'vertex '..f..':'..s..':'..i..':'..key)
                end
            end
            local ai,bi=a:getIndex(f,s),b:getIndex(f,s)
            assert((ai==nil)==(bi==nil),'index presence')
            if ai then assert(#ai==#bi);for i,v in ipairs(ai) do assert(v==bi[i],'source index') end end
            assert(a:getTexture(f,s)==b:getTexture(f,s),'diffuse material expected '..tostring(a:getTexture(f,s))..' got '..tostring(b:getTexture(f,s)))
            for _,role in ipairs({'normal','specular','emissive','mask'}) do
                assert(a:getMaterialTexture(f,s,role)==b:getMaterialTexture(f,s,role),'material '..role..' expected '..tostring(a:getMaterialTexture(f,s,role))..' got '..tostring(b:getMaterialTexture(f,s,role)))
            end
            local ay,as=a:getNormalMapSettings(f,s)
            local by,bs=b:getNormalMapSettings(f,s)
            assert(ay==by and as==bs,'normal-map properties')
        end
    end
end
local function extract(object,name)
    local source=meshDebug:new();assert(source:load(dir..name..'.msh'))
    local author=meshDebug:new();assert(author:load(object),'runtime extraction '..name)
    print('READBACK CASE '..name)
    compare(source,author)
    assert(author:save(dir..name..'-readback.msh',false,false,true))
    local again=meshDebug:new();assert(again:load(dir..name..'-readback.msh'))
    compare(source,again)
    local second=meshDebug:new();assert(second:load(object),'repeat extraction')
    compare(author,second)
end
function onInitScene()
    started=mbm.getTimeRun()
    local ok,err=pcall(function()
        mbm.addPath(dir)
        local multiple=meshDebug:new();assert(multiple:load(dir..'indexed.msh'))
        multiple:copySubsetFrom(1,multiple,1,1)
        assert(multiple:save(dir..'indexed-subsets.msh',false,false,true))
        local frames=meshDebug:new();assert(frames:load(dir..'author-subsets.msh'))
        frames:copyFrameFrom(frames,1)
        assert(frames:save(dir..'multiple-frames.msh',false,false,true))
        local unused=meshDebug:new();unused:setType('mesh')
        local f=unused:addFrame(3);local s=unused:addSubSet(f)
        assert(unused:addVertex(f,s,{
            {x=0,y=0,z=0,nx=0,ny=0,nz=1,u=0,v=0},
            {x=1,y=0,z=0,nx=0,ny=0,nz=1,u=1,v=0},
            {x=0,y=1,z=0,nx=0,ny=0,nz=1,u=0,v=1},
            {x=2,y=2,z=0,nx=0,ny=0,nz=1,u=1,v=1}}))
        assert(unused:addIndex(f,s,{1,2,3}))
        assert(unused:addAnim('Static',1,1,1,0))
        assert(unused:save(dir..'unused-vertex.msh',false,false,true))
        for _,name in ipairs({'mirrored-seam','indexed-subsets','multiple-frames','unused-vertex','no-map','prepared','imported','author-subsets','indexed','material-imported'}) do
            local object=mesh:new('3d');objects[#objects+1]=object
            assert(object:load(dir..name..'.msh'))
            extract(object,name)
            if name=='mirrored-seam' then
                local shared=mesh:new('3d');objects[#objects+1]=shared
                assert(shared:load(dir..name..'.msh'))
                local edited=meshDebug:new();assert(edited:load(object))
                assert(edited:prepareNormalMap(1,1,'generate'))
                extract(shared,name)
                extract(object,name)
            end
        end
        for _,name in ipairs({'author-import','material-async'}) do
            local object=mesh:new('3d');objects[#objects+1]=object
            remaining=remaining+1
            object:loadAsync(dir..name..'.msh',function(self,success)
                local good,why=pcall(function() assert(success);extract(self,name) end)
                if not good then failed=true;print('NORMAL MAP READBACK FAIL: '..tostring(why)) end
                remaining=remaining-1
            end)
        end
    end)
    if not ok then failed=true;print('NORMAL MAP READBACK FAIL: '..tostring(err)) end
end
function onLoop()
    if mbm.getTimeRun()-started>6 or (remaining==0 and mbm.getTimeRun()-started>1) then
        if remaining~=0 then failed=true end
        print(failed and 'NORMAL MAP READBACK FAIL' or 'NORMAL MAP READBACK PASS')
        mbm.quit()
    end
end
