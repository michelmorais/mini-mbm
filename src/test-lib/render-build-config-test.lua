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

-- Set MBM_EXPECT_NORMAL_MAPPING_3D=0/1 and MBM_EXPECT_MAX_LIGHTS=1..4.
-- Require the PASS sentinel; Lua exceptions do not determine the process exit code.
function onInitScene()
    local ok,err=pcall(function()
        local expected=assert(tonumber(os.getenv('MBM_EXPECT_NORMAL_MAPPING_3D')))
        local cap=assert(tonumber(os.getenv('MBM_EXPECT_MAX_LIGHTS')))
        local backend=os.getenv('MBM_EXPECT_BACKEND')
        if backend then
            local defines={gles='USE_OPENGL_ES',dx9='USE_DIRECTX9',dx11='USE_DIRECTX11',metal='USE_METAL'}
            assert(defines[backend] and mbm.get(defines[backend]), 'unexpected engine backend')
        end
        assert(mbm.isNormalMapping3DCompiled()==(expected==1),'engine build capability')
        for _,target in ipairs({'3d','2dw'}) do
            assert(mbm.getSupportedMaxLights(target)==cap,'compiled light cap')
            mbm.setRequestedMaxLights(target,cap)
            assert(mbm.getValidatedMaxLights(target)==cap,'validated light cap')
            assert(not pcall(mbm.setRequestedMaxLights,target,cap+1),'excess light cap accepted')
        end
        local shaders=mbm.getShaderList(true,'lit textured.ps',false,false,true)
        local code=assert(shaders[1] and shaders[1].code,'reserved lighting shader source')
        assert(code:find('i < '..cap,1,true),'fixed shader loop does not match compiled cap')
        if not mbm.get('USE_METAL') then
            for _,name in ipairs({'LightColor','LightRadius','LightPositionView'}) do
                assert(code:find(name..'['..cap..']',1,true),'fixed shader array '..name)
            end
        end
        if mbm.get('USE_OPENGL_ES') or mbm.get('USE_DIRECTX11') then
            assert((code:find('mbmMappedNormal',1,true)~=nil)==(expected==1),'fixed shader normal-map helper')
            assert((code:find('NormalMapSettings',1,true)~=nil)==(expected==1),'fixed shader normal-map constants')
        end
        print('RENDER BUILD CONFIG PASS normal='..expected..' lights='..cap)
    end)
    if not ok then print('RENDER BUILD CONFIG FAIL '..tostring(err)) end
    mbm.quit()
end
