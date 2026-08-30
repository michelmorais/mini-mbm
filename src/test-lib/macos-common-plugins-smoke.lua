--[[--------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                     |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                            |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated          |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and      |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                    |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO     |
| THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS|
| OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR   |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.      |
|----------------------------------------------------------------------------------------------------------------------]]

local imgui = require "ImGui"
local sqlite3 = require "lsqlite3"
local tilemap = require "tilemap"
local start_time = nil
local frame_count = 0

function onInitScene()
    assert(type(imgui) == "table", "ImGui did not return a Lua table")
    assert(type(tilemap) == "table", "tilemap did not return a Lua table")
    local db = assert(sqlite3.open(":memory:"), "lsqlite3 failed to open an in-memory database")
    assert(db:exec("CREATE TABLE smoke(value INTEGER); INSERT INTO smoke VALUES(42);") == sqlite3.OK,
           "lsqlite3 failed to execute SQL")
    db:close()
    start_time = mbm.getTimeRun()
end

function onLoop()
    frame_count = frame_count + 1
    if mbm.getTimeRun() - start_time >= 0.25 then
        print("info", "green", "MACOS_COMMON_PLUGINS_SMOKE_OK frames=" .. tostring(frame_count))
        mbm.quit()
    end
end
