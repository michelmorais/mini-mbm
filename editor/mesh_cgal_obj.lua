--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]


-- OBJ interchange shared by the editors and the standalone tool fixtures.
local M={}
local function newFile(path)
    local existing=io.open(path,'rb')
    if existing then existing:close();error('Output already exists: '..path) end
end
function M.export(asset,path,selectedSubset)
    assert(path:match('%.obj$'),'expected .obj output')
    local materialPath=path:sub(1,-5)..'.mtl'
    newFile(path);newFile(materialPath)
    local vertices,positions,uvs,faces,materials={},{},{},{},{}
    local subsets={}
    if selectedSubset then subsets[1]=selectedSubset
    else for subset=1,asset:getTotalSubset(1) do subsets[#subsets+1]=subset end end
    for _,subset in ipairs(subsets) do
        local list=asset:getVertex(1,subset,1,asset:getTotalVertex(1,subset))
        local indices=asset:getIndex(1,subset)
        if not indices or #indices==0 then indices={};for i=1,#list do indices[i]=i end end
        local remap,uvmap={},{}
        for i,v in ipairs(list) do
            local key=string.format('%.17g %.17g %.17g',v.x+0.0,v.y+0.0,v.z+0.0)
            if not positions[key] then vertices[#vertices+1]='v '..key;positions[key]=#vertices end
            remap[i]=positions[key]
            assert(v.u and v.v,'missing source UV')
            uvs[#uvs+1]=string.format('vt %.17g %.17g',v.u,1-v.v);uvmap[i]=#uvs
        end
        local name='subset_'..subset
        faces[#faces+1]='usemtl '..name
        for i=1,#indices,3 do
            local corners={}
            for j=0,2 do local index=indices[i+j];corners[#corners+1]=remap[index]..'/'..uvmap[index] end
            faces[#faces+1]='f '..table.concat(corners,' ')
        end
        local textureName=asset:getTexture(1,subset)
        materials[#materials+1]='newmtl '..name
        materials[#materials+1]='Kd 1 1 1'
        if textureName and textureName~='' then
            if textureName:sub(1,1)=='#' then
                local r,g,b=textureName:match('^#(%x%x)(%x%x)(%x%x)')
                assert(r,'invalid solid-color texture')
                materials[#materials]=string.format('Kd %.9g %.9g %.9g',tonumber(r,16)/255,tonumber(g,16)/255,tonumber(b,16)/255)
                materials[#materials+1]=string.format('d %.9g',(tonumber(textureName:sub(8,9),16) or 255)/255)
            else materials[#materials+1]='map_Kd '..assert(mbm.getFullPath(textureName),'missing source texture') end
        end
    end
    local materialFile=assert(io.open(materialPath,'w'));assert(materialFile:write(table.concat(materials,'\n'),'\n'));materialFile:close()
    local file=assert(io.open(path,'w'))
    assert(file:write('mtllib ',materialPath:match('[^/\\]+$'),'\n',table.concat(vertices,'\n'),'\n',table.concat(uvs,'\n'),'\n',table.concat(faces,'\n'),'\n'));file:close()
    return #vertices,(#faces-#subsets)
end
local function readMaterials(path)
    local materials,current={},nil
    local file=assert(io.open(path,'r'))
    for line in file:lines() do
        local tag,value=line:match('^(%S+)%s*(.*)$')
        if tag=='newmtl' then current={};materials[value]=current
        elseif tag=='map_Kd' then current.texture=value
        elseif tag=='d' then current.opacity=assert(tonumber(value))
        elseif tag=='Kd' then
            local r,g,b=value:match('(%S+)%s+(%S+)%s+(%S+)')
            current.color={tonumber(r),tonumber(g),tonumber(b)}
        end
    end
    file:close();return materials
end
function M.read(path)
    local file=assert(io.open(path,'r'))
    local positions,uvs,groups,byMaterial,materials={},{},{},{},{}
    local material=''
    for line in file:lines() do
        local tag,value=line:match('^(%S+)%s*(.*)$')
        if tag=='mtllib' then
            local library=value
            if not library:match('^/') and not library:match('^%a:') then library=(path:match('^(.*[/\\])') or '')..library end
            for name,item in pairs(readMaterials(library)) do materials[name]=item end
        elseif tag=='v' then
            local x,y,z=value:match('(%S+)%s+(%S+)%s+(%S+)')
            positions[#positions+1]={x=assert(tonumber(x)),y=assert(tonumber(y)),z=assert(tonumber(z))}
        elseif tag=='vt' then
            local u,v=value:match('(%S+)%s+(%S+)');uvs[#uvs+1]={u=assert(tonumber(u)),v=1-assert(tonumber(v))}
        elseif tag=='usemtl' then material=value
        elseif tag=='f' then
            local group=byMaterial[material]
            if not group then group={name=material,vertices={},indices={},keys={}};groups[#groups+1]=group;byMaterial[material]=group end
            local count=0
            for pair in value:gmatch('%S+') do
                count=count+1
                local vi,ti=pair:match('^(%d+)/(%d+)$');vi,ti=assert(tonumber(vi)),assert(tonumber(ti))
                local point,uv=assert(positions[vi]),assert(uvs[ti])
                local key=string.format('%.17g %.17g %.17g %.17g %.17g',point.x,point.y,point.z,uv.u,uv.v)
                local index=group.keys[key]
                if not index then
                    local p,t=assert(positions[vi]),assert(uvs[ti]);index=#group.vertices+1
                    group.vertices[index]={x=p.x,y=p.y,z=p.z,u=t.u,v=t.v};group.keys[key]=index
                end
                group.indices[#group.indices+1]=index
            end
            assert(count==3,'expected triangles')
        end
    end
    file:close()
    return {groups=groups,positions=positions,materials=materials}
end
function M.asset(data)
    local d=meshDebug:new();d:setType('mesh');d:setModeDraw('TRIANGLES');d:addFrame(3)
    for i,group in ipairs(data.groups) do
        d:addSubSet(1);assert(d:addVertex(1,i,group.vertices));assert(d:addIndex(1,i,group.indices))
        local material=data.materials[group.name] or {}
        local texture=material.texture
        if not texture then
            local c=material.color or {1,1,1};texture=string.format('#%02X%02X%02X%02X',math.floor(c[1]*255+.5),math.floor(c[2]*255+.5),math.floor(c[3]*255+.5),math.floor((material.opacity or 1)*255+.5))
        end
        d:setTexture(1,i,texture)
    end
    d:addAnim('Static',1,1,1,0);d:removeNormals();assert(d:check())
    return d
end
return M
