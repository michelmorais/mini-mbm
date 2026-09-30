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

local objects = {}
local remaining = 0
local failed = false
local started = 0
local dir = assert(os.getenv('MBM_NORMAL_MAP_FIXTURE_DIR'), 'set MBM_NORMAL_MAP_FIXTURE_DIR')..'/'
local function check(condition, label)
    if not condition then failed = true; print('NORMAL MAP RUNTIME FAIL: '..label) end
end
function onInitScene()
    started = mbm.getTimeRun()
    local ok, err = pcall(function()
        mbm.addPath(dir)
        for _, name in ipairs({'no-map', 'prepared', 'unprepared', 'reordered', 'shared-uv', 'mixed-uv', 'imported', 'material-no-map', 'material-first', 'material-imported'}) do
            local m = mesh:new('3d')
            objects[#objects+1] = m
            assert(m:load(dir..name..'.msh'), name)
            if name == 'unprepared' or name == 'prepared' then
                local extracted=meshDebug:new()
                assert(extracted:load(m),'runtime basis extraction')
                local report=assert(extracted:prepareNormalMap(1,1,'preserve'))
                check(report.reused == (name == 'prepared' or mbm.isNormalMapping3DCompiled()),
                    'runtime preparation follows build; persisted basis retained: '..name)
            end
            local convention, strength = m:getNormalMapSettings()
            if name == 'material-imported' then
                check(convention == '-Y' and strength == 3, name..' properties')
            elseif name == 'material-no-map' or name == 'material-first' then
                check(convention == '-Y' and strength == 2.5, name..' properties')
            else
                check(convention == '+Y' and strength == 1, name..' defaults')
            end

        end
        for _, name in ipairs({'duplicate','bad-frame','bad-version','stale-source','bad-count','bad-source-index','nan-tangent','bad-sign','truncated','trailing','bad-local-index','wrong-triangle', 'material-duplicate-section', 'material-bad-frame', 'material-bad-subset', 'material-bad-convention', 'material-negative-strength', 'material-nan', 'material-infinity', 'material-count', 'material-version', 'material-truncated', 'material-trailing', 'material-duplicate-entry'}) do
            local m = mesh:new('3d')
            objects[#objects+1] = m
            check(not m:load(dir..name..'.msh'), 'accepted '..name)
        end
        -- These paths have not been loaded, so they exercise the worker parser, not a cache hit.
        for _, name in ipairs({'roundtrip', 'imported-roundtrip', 'edited', 'regenerated', 'indexed-imported', 'bad-frame', 'material-async', 'material-compressed', 'material-bad-subset'}) do
            local m = mesh:new('3d')
            objects[#objects+1] = m
            remaining = remaining+1
            m:loadAsync(dir..name..'.msh', function(self, success)
                local okCallback, errCallback = pcall(function()
                    check(success == (name ~= 'bad-frame' and name ~= 'material-bad-subset'), 'async '..name)
                    if success and name == 'material-compressed' then
                        local convention, strength = self:getNormalMapSettings()
                        check(convention == '-Y' and strength == 2.5, 'async compressed properties')
                    end
                    if success and name == 'material-async' then
                        local convention, strength = self:getNormalMapSettings()
                        check(convention == '-Y' and strength == 3, 'async material settings')
                        assert(self:setNormalMapSettings('+Y', 0))
                        convention, strength = self:getNormalMapSettings()
                        check(convention == '+Y' and strength == 0, 'runtime setting mutation')
                        check(not self:setNormalMapSettings('-Y', 1, 999), 'runtime invalid subset')
                        for _, args in ipairs({{'unknown', 1}, {'+Y', -1}, {'+Y', math.huge}, {'+Y', 0/0}}) do
                            check(not pcall(self.setNormalMapSettings, self, args[1], args[2]), 'runtime invalid value')
                        end
                        local author = meshDebug:new()
                        assert(author:load(self), 'material runtime extraction')
                        convention, strength = author:getNormalMapSettings(1,1)
                        check(convention == '+Y' and strength == 0, 'runtime extraction preserves updated properties')
                        assert(author:setNormalMapSettings(1,1,'-Y',2))
                        check(not author:setNormalMapSettings(1,999,'+Y',1), 'author invalid subset')
                        check(author:getNormalMapSettings(1,999) == nil, 'author missing subset')
                        for _, args in ipairs({{'unknown',1},{'+Y',-1},{'+Y',math.huge},{'+Y',0/0}}) do
                            check(not pcall(author.setNormalMapSettings,author,1,1,args[1],args[2]),'author invalid value')
                        end
                        assert(author:save(dir..'material-runtime.msh',false,false,true))
                        local reloaded = meshDebug:new()
                        assert(reloaded:load(dir..'material-runtime.msh'))
                        convention, strength = reloaded:getNormalMapSettings(1,1)
                        check(convention == '-Y' and strength == 2,'Lua authored material round-trip')
                        local sibling = mesh:new('3d')
                        objects[#objects+1] = sibling
                        assert(sibling:load(dir..name..'.msh'))
                        convention, strength = sibling:getNormalMapSettings()
                        check(convention == '+Y' and strength == 0,'shared asset material state')
                    end
                    if success and name == 'indexed-imported' then
                        local author = meshDebug:new()
                        assert(author:load(self), 'indexed runtime extraction')
                        assert(author:save(dir..name..'-runtime.msh', false, false, true))
                    end

                end)
                if not okCallback then failed = true; print('NORMAL MAP RUNTIME FAIL: '..tostring(errCallback)) end
                remaining = remaining-1
            end)
        end
    end)
    if not ok then failed = true; print('NORMAL MAP RUNTIME FAIL: '..tostring(err)) end
end
function onLoop(delta)
    if mbm.getTimeRun()-started > 6 or (remaining == 0 and mbm.getTimeRun()-started > 1) then
        check(remaining == 0, 'async timeout')
        print(failed and 'NORMAL MAP RUNTIME FAIL' or 'NORMAL MAP RUNTIME PASS')
        mbm.quit()
    end
end
