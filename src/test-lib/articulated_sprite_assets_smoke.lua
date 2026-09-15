--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
|------------------------------------------------------------------------------------------------------------------------|

]]--

package.path='editor/?.lua;'..package.path
local IO=require 'articulated_sprite_io'
local Model=require 'articulated_sprite_model'
local files,dirs={},{}
local function write(path,bytes)
    local f=assert(io.open(path,'wb')); assert(f:write(bytes)); assert(f:close())
    files[path]=true
end
local function read(path)
    local f=assert(io.open(path,'rb')); local bytes=f:read('a'); f:close(); return bytes
end
local function mkdir(path) assert(mbm.createDirectories(path)); dirs[#dirs+1]=path end
local completed=false
function onInitScene()
    local root=os.tmpname(); os.remove(root); mkdir(root)
    mkdir(root..'/a'); mkdir(root..'/b')
    local assets=root..'/project.asprite.assets'; mkdir(assets)
    write(root..'/a/Hammer.png','first')
    write(root..'/b/Hammer.png','second')
    write(root..'/a/Hammer_2.png','third')
    write(assets..'/Hammer.png','unrelated resource')
    local project=Model.new()
    for _,p in ipairs({'a/Hammer.png','b/Hammer.png','a/Hammer_2.png'}) do
        project.images[#project.images+1]={path=root..'/'..p,width=1,height=1}
    end
    local path=root..'/project.asprite'; files[path]=true; files[path..'.previous']=true
    IO.save(project,path)
    local loaded=IO.load(path)
    for i,base in ipairs({'Hammer_3.png','Hammer_4.png','Hammer_2.png'}) do
        assert(loaded.images[i].path==assets..'/'..base)
        files[loaded.images[i].path]=true
        assert(read(loaded.images[i].path)==read(project.images[i].path))
    end
    assert(read(assets..'/Hammer.png')=='unrelated resource')
    local first=read(path)
    IO.save(project,path); assert(read(path)==first,'repeated save changed names')
    IO.save(loaded,path); assert(read(path)==first,'loaded project changed names')
    assert(project.images[1].path==root..'/a/Hammer.png','save mutated source paths')
    -- A separate project retains original names and is independent of the first.
    local second=root..'/second.asprite'; files[second]=true
    IO.save(project,second)
    local other=IO.load(second); dirs[#dirs+1]=root..'/second.asprite.assets'
    assert(other.images[1].path:match('/Hammer%.png$'))
    for _,img in ipairs(other.images) do files[img.path]=true end
    for file in pairs(files) do os.remove(file) end
    for i=#dirs,1,-1 do os.remove(dirs[i]) end
    print('ASSET NAMES / COLLISIONS / REPEATED SAVE / SEPARATE PROJECTS OK')
    completed=true; mbm.quit()
end
function onLoop() assert(completed,'asset test initialization failed'); mbm.quit() end
