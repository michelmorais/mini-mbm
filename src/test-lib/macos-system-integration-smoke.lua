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

local start_time = nil
local stage = "init"
local exit_job = nil
local argument_job = nil
local cancel_job = nil
local marker = nil
local failure = nil

local function check(condition, message)
    if not condition then
        error(message, 2)
    end
end

local function initialize()
    marker = "/tmp/mini-mbm argumento com espaco e Unicode á " .. tostring(mbm.getTimeRun()) .. ".txt"
    local executable_path = mbm.getExecutablePath()
    check(type(executable_path) == "string" and executable_path:sub(1, 1) == "/",
          "getExecutablePath did not return an absolute path")
    check(mbm.existFile(executable_path), "getExecutablePath returned a path that does not exist")

    local missing_job, spawn_error = mbm.executeProcessAsync({
        executable = "/tmp/mini-mbm-executable-that-must-not-exist"
    })
    check(missing_job == nil and type(spawn_error) == "string",
          "a missing executable did not return nil plus an error")

    exit_job = assert(mbm.executeProcessAsync({
        executable = "/bin/sh",
        arguments = {"-c", "exit 37"}
    }))
    argument_job = assert(mbm.executeProcessAsync({
        executable = "/bin/sh",
        arguments = {
            "-c",
            "[ \"$1\" = \"valor com espaco e Unicode á\" ] && /usr/bin/touch \"$2\"",
            "mini-mbm-system-smoke",
            "valor com espaco e Unicode á",
            marker
        }
    }))
    cancel_job = assert(mbm.executeProcessAsync({
        executable = "/bin/sleep",
        arguments = {"10"}
    }))
    check(cancel_job:isRunning(), "sleep process was not running before cancellation")
    check(cancel_job:cancel(), "process cancellation failed")
    stage = "wait"
end

function onInitScene()
    start_time = mbm.getTimeRun()
    local ok, message = pcall(initialize)
    if not ok then
        failure = message
    end
end

function onLoop()
    if failure then
        print("error", "red", "MACOS_SYSTEM_INTEGRATION_SMOKE_FAIL " .. tostring(failure))
        mbm.quit()
        return
    end

    if mbm.getTimeRun() - start_time > 5 then
        print("error", "red", "MACOS_SYSTEM_INTEGRATION_SMOKE_FAIL timeout at stage=" .. stage)
        mbm.quit()
        return
    end

    if stage ~= "wait" or exit_job:isRunning() or argument_job:isRunning() or cancel_job:isRunning() then
        return
    end

    local ok, message = pcall(function()
        check(exit_job:getExitCode() == 37, "normal process exit code was not preserved")
        check(argument_job:getExitCode() == 0, "process with Unicode/space arguments failed")
        check(mbm.existFile(marker), "Unicode/space argument marker was not created")
        check(os.remove(marker), "temporary Unicode/space marker could not be removed")
        check(cancel_job:getExitCode() == 143, "cancelled process did not report SIGTERM exit code 143")
        exit_job:destroy()
        argument_job:destroy()
        cancel_job:destroy()
        check(exit_job:getExitCode() == nil, "destroyed job unexpectedly retained an exit code")
    end)
    if not ok then
        print("error", "red", "MACOS_SYSTEM_INTEGRATION_SMOKE_FAIL " .. tostring(message))
        mbm.quit()
        return
    end

    print("info", "green", "MACOS_SYSTEM_INTEGRATION_SMOKE_OK")
    mbm.quit()
end
