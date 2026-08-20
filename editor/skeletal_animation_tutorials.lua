--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ALL   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Data-driven tutorials for the Skeletal Animation Editor.
]]--

local M={items={},byId={},activeId=nil,activeStep=1,pendingFocus=nil}

function M.register(tutorial)
    assert(type(tutorial)=='table','tutorial must be a table')
    assert(type(tutorial.id)=='string' and tutorial.id~='','tutorial id is required')
    assert(type(tutorial.menuKey)=='string' and tutorial.menuKey~='','tutorial menuKey is required')
    assert(type(tutorial.titleKey)=='string' and tutorial.titleKey~='','tutorial titleKey is required')
    assert(type(tutorial.steps)=='table' and #tutorial.steps>0,'tutorial steps are required')
    assert(not M.byId[tutorial.id],'duplicate tutorial id: '..tutorial.id)
    for index,step in ipairs(tutorial.steps) do
        assert(type(step.titleKey)=='string' and step.titleKey~='',
            string.format('tutorial step %d titleKey is required',index))
        assert(type(step.bodyKey)=='string' and step.bodyKey~='',
            string.format('tutorial step %d bodyKey is required',index))
    end
    M.items[#M.items+1]=tutorial
    M.byId[tutorial.id]=tutorial
    return tutorial
end

function M.open(id)
    if not M.byId[id] then return false end
    M.activeId=id
    M.activeStep=1
    return true
end

function M.close()
    M.activeId=nil
    M.activeStep=1
    M.pendingFocus=nil
end

function M.requestFocus(focus)
    M.pendingFocus=focus
end

function M.consumeFocus(focus)
    if not focus or M.pendingFocus~=focus then return false end
    M.pendingFocus=nil
    return true
end

function M.renderMenu(gui,lang)
    if not gui.BeginMenu(lang.L('swl_tutorial_menu')) then return end
    local openedTutorial=nil
    for _,tutorial in ipairs(M.items) do
        local selected=M.activeId==tutorial.id
        local pressed,checked=gui.MenuItem(lang.L(tutorial.menuKey),nil,selected)
        if pressed then
            if checked==false and selected then
                M.close()
            else
                M.open(tutorial.id)
                openedTutorial=tutorial
            end
        end
    end
    if M.activeId then
        gui.Separator()
        if gui.MenuItem(lang.L('swl_tutorial_close')) then M.close() end
    end
    gui.EndMenu()
    return openedTutorial
end

function M.renderWindow(gui,lang,leftPanelRight,hasMesh)
    local tutorial=M.activeId and M.byId[M.activeId] or nil
    if not tutorial then return nil end
    local screenWidth,screenHeight=mbm.getRealSizeScreen()
    local x=math.max(0,screenWidth-380)
    gui.SetNextWindowPos({x=x,y=28},gui.Flags('ImGuiCond_Once'))
    gui.SetNextWindowSize({x=370,y=math.max(420,math.min(650,screenHeight-40))},
        gui.Flags('ImGuiCond_Once'))
    gui.SetNextWindowSizeConstraints({x=320,y=320},{x=620,y=math.max(320,screenHeight-28)})
    local opened,closed=gui.Begin(lang.L(tutorial.titleKey)..'##swlTutorialWindow',true,
        gui.Flags('ImGuiWindowFlags_NoCollapse'))
    local navigation=nil
    if opened then
        gui.TextWrapped(lang.L(tutorial.introKey))
        gui.Separator()
        gui.Text(lang.L('swl_tutorial_steps'))
        for index,step in ipairs(tutorial.steps) do
            local prefix=index==M.activeStep and '> ' or ''
            if gui.Button(string.format('%s%d. %s##swlTutorialStep%d',prefix,index,
                    lang.L(step.titleKey),index)) then
                M.activeStep=index
            end
        end
        gui.Separator()
        local step=tutorial.steps[M.activeStep]
        gui.TextColored({r=0.2,g=0.85,b=1,a=1},string.format(
            lang.L('swl_tutorial_step_fmt'),M.activeStep,#tutorial.steps,lang.L(step.titleKey)))
        gui.TextWrapped(lang.L(step.bodyKey))
        if step.workspace then
            gui.BeginDisabled(step.requiresMesh==true and not hasMesh)
            if gui.Button(lang.L('swl_tutorial_go_to_workspace')) then
                navigation={workspace=step.workspace,statusKey=step.bodyKey,focus=step.focus}
            end
            gui.EndDisabled()
            if step.requiresMesh==true and not hasMesh then
                gui.TextDisabled(lang.L('swl_tutorial_open_mesh_first'))
            end
        else
            gui.TextDisabled(lang.L('swl_tutorial_use_menu_instruction'))
        end
        gui.Separator()
        gui.BeginDisabled(M.activeStep<=1)
        if gui.Button(lang.L('swl_tutorial_previous')) then M.activeStep=M.activeStep-1 end
        gui.EndDisabled()
        gui.SameLine()
        gui.BeginDisabled(M.activeStep>=#tutorial.steps)
        if gui.Button(lang.L('swl_tutorial_next')) then M.activeStep=M.activeStep+1 end
        gui.EndDisabled()
        if step.checkKey then
            gui.Separator()
            gui.TextColored({r=1,g=0.75,b=0.15,a=1},lang.L('swl_tutorial_check_before_next'))
            gui.TextWrapped(lang.L(step.checkKey))
        end
    end
    gui.End()
    if closed then M.close() end
    return navigation
end

M.register({
    id='tutorial_1',
    menuKey='swl_tutorial_1_menu',
    titleKey='swl_tutorial_1_title',
    introKey='swl_tutorial_1_intro',
    steps={
        {titleKey='swl_tutorial_1_step_1_title',bodyKey='swl_tutorial_1_step_1_body',
            checkKey='swl_tutorial_1_step_1_check'},
        {titleKey='swl_tutorial_1_step_2_title',bodyKey='swl_tutorial_1_step_2_body',
            checkKey='swl_tutorial_1_step_2_check',workspace='bone_editor',focus='bone_create',
            requiresMesh=true},
        {titleKey='swl_tutorial_1_step_3_title',bodyKey='swl_tutorial_1_step_3_body',
            checkKey='swl_tutorial_1_step_3_check',workspace='bind',focus='bind_hierarchy',
            requiresMesh=true},
        {titleKey='swl_tutorial_1_step_4_title',bodyKey='swl_tutorial_1_step_4_body',
            checkKey='swl_tutorial_1_step_4_check',workspace='bone_editor',focus='bone_weights',
            requiresMesh=true},
        {titleKey='swl_tutorial_1_step_5_title',bodyKey='swl_tutorial_1_step_5_body',
            checkKey='swl_tutorial_1_step_5_check',workspace='paint',focus='paint_mask',
            requiresMesh=true},
        {titleKey='swl_tutorial_1_step_6_title',bodyKey='swl_tutorial_1_step_6_body',
            checkKey='swl_tutorial_1_step_6_check',workspace='animation',focus='animation_clip',
            requiresMesh=true},
        {titleKey='swl_tutorial_1_step_7_title',bodyKey='swl_tutorial_1_step_7_body',
            checkKey='swl_tutorial_1_step_7_check',workspace='runtime',focus='runtime_preview',
            requiresMesh=true},
        {titleKey='swl_tutorial_1_step_8_title',bodyKey='swl_tutorial_1_step_8_body',
            checkKey='swl_tutorial_1_step_8_check'},
    },
})

M.register({
    id='tutorial_2',
    menuKey='swl_tutorial_2_menu',
    titleKey='swl_tutorial_2_title',
    introKey='swl_tutorial_2_intro',
    assetFactory='worm_cylinder',
    steps={
        {titleKey='swl_tutorial_2_step_1_title',bodyKey='swl_tutorial_2_step_1_body',
            checkKey='swl_tutorial_2_step_1_check'},
        {titleKey='swl_tutorial_2_step_2_title',bodyKey='swl_tutorial_2_step_2_body',
            checkKey='swl_tutorial_2_step_2_check',workspace='bone_editor',focus='bone_create',
            requiresMesh=true},
        {titleKey='swl_tutorial_2_step_3_title',bodyKey='swl_tutorial_2_step_3_body',
            checkKey='swl_tutorial_2_step_3_check',workspace='bind',focus='bind_hierarchy',
            requiresMesh=true},
        {titleKey='swl_tutorial_2_step_4_title',bodyKey='swl_tutorial_2_step_4_body',
            checkKey='swl_tutorial_2_step_4_check',workspace='bone_editor',focus='bone_weights',
            requiresMesh=true},
        {titleKey='swl_tutorial_2_step_5_title',bodyKey='swl_tutorial_2_step_5_body',
            checkKey='swl_tutorial_2_step_5_check',workspace='paint',focus='paint_mask',
            requiresMesh=true},
        {titleKey='swl_tutorial_2_step_6_title',bodyKey='swl_tutorial_2_step_6_body',
            checkKey='swl_tutorial_2_step_6_check',workspace='animation',focus='animation_clip',
            requiresMesh=true},
        {titleKey='swl_tutorial_2_step_7_keys_title',bodyKey='swl_tutorial_2_step_7_keys_body',
            checkKey='swl_tutorial_2_step_7_keys_check',workspace='animation',
            focus='animation_clip',requiresMesh=true},
        {titleKey='swl_tutorial_2_step_7_title',bodyKey='swl_tutorial_2_step_7_body',
            checkKey='swl_tutorial_2_step_7_check',workspace='runtime',focus='runtime_preview',
            requiresMesh=true},
        {titleKey='swl_tutorial_2_step_8_title',bodyKey='swl_tutorial_2_step_8_body',
            checkKey='swl_tutorial_2_step_8_check'},
    },
})

return M
