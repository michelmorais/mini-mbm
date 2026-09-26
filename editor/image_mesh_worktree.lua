--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                         |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Lazy Image Mesh project loading and temporary mesh generation for Mesh Debug.
]]--

local IO=require 'image_mesh_io'
local Model=require 'image_mesh_model'
local Generation=require 'image_mesh_generation'
local Build=require 'image_mesh_build'
local TextureAliases=require 'image_mesh_texture_aliases'

local M={}
local activeEntry=nil
local queuedEntries={}
local queuedHead=1

local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[image_mesh_worktree] '..tostring(result[2])) end
    return table.unpack(result,1,result.n)
end

local function fileName(path)
    return path:gsub('\\','/'):match('([^/]+)$') or path
end

local function directory(path)
    return path:gsub('\\','/'):match('^(.*)/[^/]*$') or '.'
end

local function normalize(path)
    return (path or ''):gsub('\\','/')
end

local function shellQuote(path)
    if package.config:sub(1,1)=='\\' then
        return '"'..tostring(path):gsub('"','\\"')..'"'
    end
    return "'"..tostring(path):gsub("'", "'\\''").."'"
end

local function removeDirectory(path)
    if not path or path=='' then return end
    if package.config:sub(1,1)=='\\' then
        os.execute('rmdir /s /q '..shellQuote(path)..' 2>nul')
    else
        os.execute('rm -rf '..shellQuote(path))
    end
end

local function useTemporaryTextureAliases(entry,asset)
    TextureAliases.apply(entry,asset,entry.tempDirectory,entry.path,true)
end

local function addProjectPaths(project,path)
    local added={}
    local function add(pathToAdd)
        local dir=directory(pathToAdd)
        local key=normalize(dir)
        if added[key] then return end
        added[key]=true
        mbm.addPath(dir)
    end
    add(path)
    add(project.image.path)
    IO.texturePaths(project,function(texture)
        add(texture)
        return texture
    end)
end

function M.load(path)
    path=normalize(path)
    local project=IO.load(path)
    addProjectPaths(project,path)
    return {
        path=path,
        name=fileName(path),
        project=project,
        status='idle',
        builtRegions={},
        suppressedRegions={},
        meshEntries={},
        tempTextureAliases={},
        nextTempTextureId=0,
        tempDirectory=nil,
        task=nil,
        worker=nil,
    }
end

function M.samePath(a,b)
    return normalize(a)==normalize(b)
end

function M.validateSource(entry)
    local image=entry.project.image
    assert(IO.exists(image.path),'ime_missing_image')
    local loaded=assert(mbm.loadTexture(image.path),'ime_invalid_image')
    assert(loaded.width==image.width and loaded.height==image.height,'ime_image_size_changed')
end

local function missingRegions(entry)
    local missing={}
    for _,region in ipairs(entry.project.regions) do
        if not entry.builtRegions[region.id] and not entry.suppressedRegions[region.id] then
            missing[#missing+1]=region
        end
    end
    return missing
end

local function createTask(entry)
    local regions=missingRegions(entry)
    if #regions==0 then
        entry.status='ready'
        return true
    end
    M.validateSource(entry)
    if not entry.tempDirectory then
        local folder=tUtil.getTemporaryFilePath('_mini_mbm_imesh')
        assert(mbm.createDirectories(folder),'ime_temporary_folder_failed')
        entry.tempDirectory=folder
        mbm.addPath(folder)
    end
    entry.worker={}
    entry.currentIndex=0
    entry.currentRegion=''
    entry.cancelRequested=nil
    entry.error=nil
    entry.status='generating'
    entry.task=coroutine.create(function()
        for index,region in ipairs(regions) do
            if entry.cancelRequested then error('ime_generation_cancelled',0) end
            if not entry.suppressedRegions[region.id] then
                entry.currentIndex=index
                entry.currentRegion=region.name
                local asset,report,options=Build.generate(entry.worker,entry.project,region)
                if not asset then error(report or 'image_mesh_generation_failed',0) end
                useTemporaryTextureAliases(entry,asset)
                local sep=package.config:sub(1,1)
                local output=entry.tempDirectory..sep..IO.exportName(region)
                entry.currentOutput=output
                assert(asset:save(output,false,false,true),'ime_export_failed')
                coroutine.yield({path=output,region=region,options=options,report=report})
                entry.currentOutput=nil
            end
        end
    end)
    return true
end

function M.ensure(entry)
    if entry.status=='idle' or entry.status=='queued' or
        (entry.status=='ready' and #missingRegions(entry)>0) then
        if activeEntry and activeEntry~=entry then
            entry.status='queued'
            if not entry.inQueue then
                entry.inQueue=true
                queuedEntries[#queuedEntries+1]=entry
            end
            return true
        end
        local ok,err=dpCall(createTask,entry)
        if not ok then
            entry.status='failed'
            entry.error=tostring(err)
            entry.task=nil
            entry.worker=nil
            return false,entry.error
        end
        if entry.status=='generating' then activeEntry=entry end
    end
    return entry.status~='failed',entry.error
end

function M.resume(entry)
    local task=entry.task
    if not task or (entry.status~='generating' and entry.status~='cancelling') then return nil end
    local result=table.pack(coroutine.resume(task))
    if not result[1] then
        entry.task=nil
        entry.currentOutput=nil
        local cancelled=entry.cancelRequested or (entry.worker and entry.worker.generationCancelled)
        entry.worker=nil
        if cancelled then
            entry.status='cancelled'
            entry.error=nil
        else
            entry.status='failed'
            entry.error=tostring(result[2])
        end
        if activeEntry==entry then activeEntry=nil end
        return nil
    end
    if coroutine.status(task)=='dead' then
        entry.task=nil
        entry.worker=nil
        entry.status='ready'
        if activeEntry==entry then activeEntry=nil end
    end
    return result[2]
end

function M.advance()
    if not activeEntry then
        while queuedHead<=#queuedEntries and not activeEntry do
            local entry=queuedEntries[queuedHead]
            queuedEntries[queuedHead]=false
            queuedHead=queuedHead+1
            entry.inQueue=nil
            if entry.status=='queued' then M.ensure(entry) end
        end
        if queuedHead>#queuedEntries then
            queuedEntries={}
            queuedHead=1
        end
    end
    local entry=activeEntry
    if not entry then return nil,nil end
    return entry,M.resume(entry)
end

function M.markBuilt(entry,region,path,meshEntry)
    entry.builtRegions[region.id]=path
    entry.suppressedRegions[region.id]=nil
    if meshEntry then
        entry.meshEntries=entry.meshEntries or {}
        entry.meshEntries[#entry.meshEntries+1]=meshEntry
    end
    entry.currentOutput=nil
    entry.error=nil
end

function M.markFailed(entry,message)
    entry.task=nil
    entry.currentOutput=nil
    entry.worker=nil
    entry.error=tostring(message or 'image_mesh_load_failed')
    entry.status='failed'
    if activeEntry==entry then activeEntry=nil end
end

function M.forgetRegion(entry,regionId)
    if not entry then return end
    entry.builtRegions[regionId]=nil
    entry.suppressedRegions[regionId]=true
    if entry.meshEntries then
        for i=#entry.meshEntries,1,-1 do
            if entry.meshEntries[i].imageMeshRegionId==regionId then
                table.remove(entry.meshEntries,i)
            end
        end
    end
end

function M.retry(entry)
    if entry.status=='generating' then return false end
    entry.status='idle'
    entry.error=nil
    return M.ensure(entry)
end

function M.cancel(entry)
    if not entry then return false end
    if entry.status=='queued' then
        entry.status='cancelled'
        entry.inQueue=nil
        entry.error=nil
        return true
    end
    if entry.status~='generating' then return false end
    entry.cancelRequested=true
    Generation.cancel(entry.worker)
    entry.status='cancelling'
    return true
end

function M.progress(entry)
    local worker=entry.worker or {}
    return worker.simplifyProgress or worker.generationProgress or 0
end

function M.dispose(entry)
    if not entry then return end
    if entry.status=='generating' or entry.status=='cancelling' then
        Generation.cancel(entry.worker)
    end
    entry.task=nil
    entry.worker=nil
    entry.inQueue=nil
    if activeEntry==entry then activeEntry=nil end
    removeDirectory(entry.tempDirectory)
    entry.tempDirectory=nil
    entry.status='disposed'
end

function M.fitDistance(project,region)
    local options=Model.options(project,region)
    local extent=math.max(options.width,options.height,options.depth+options.relief)
    if options.heightSource=='curved' then
        extent=math.max(extent,select(2,Model.curved.range(options)))
    end
    return math.max(10,extent*2.7)
end

function M.displayError(entry)
    local message=tostring(entry.error or '')
    local key=message:match('(ime_[%w_]+)')
    if key then
        local translated=tLang.L(key)
        if translated~=key then return translated end
    end
    return message
end

return M
