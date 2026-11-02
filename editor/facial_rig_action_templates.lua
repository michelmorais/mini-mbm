--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
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
|------------------------------------------------------------------------------------------------------------------------|

   FACS-inspired facial action templates for the Facial Rig Editor.
]]--

local M={}
local tPointGizmo=require 'facial_surface_point_gizmo'

local AREA_COLOR={r=0.15,g=0.85,b=1,a=1}
local ACTION_LINE_COLOR={r=1,g=0.46,b=0.08,a=1}
local CONTROLLER_COLOR={r=0.28,g=1,b=0.32,a=1}
local LINE_CONTROLLER_COLOR={r=1,g=0.9,b=0.16,a=1}

local FACS_ACTION_UNIT_CATALOG={
    {code='AU1',nameEn='Inner Brow Raiser',namePt='Elevador interno da sobrancelha'},
    {code='AU2',nameEn='Outer Brow Raiser',namePt='Elevador externo da sobrancelha'},
    {code='AU4',nameEn='Brow Lowerer',namePt='Abaixador da sobrancelha'},
    {code='AU5',nameEn='Upper Lid Raiser',namePt='Elevador da palpebra superior'},
    {code='AU6',nameEn='Cheek Raiser',namePt='Elevador da bochecha'},
    {code='AU7',nameEn='Lid Tightener',namePt='Apertador das palpebras'},
    {code='AU8',nameEn='Lips Toward Each Other',namePt='Aproximacao dos labios'},
    {code='AU9',nameEn='Nose Wrinkler',namePt='Enrugador do nariz'},
    {code='AU10',nameEn='Upper Lip Raiser',namePt='Elevador do labio superior'},
    {code='AU11',nameEn='Nasolabial Deepener',namePt='Aprofundador do sulco nasolabial'},
    {code='AU12',nameEn='Lip Corner Puller',namePt='Puxador do canto dos labios'},
    {code='AU13',nameEn='Cheek Puffer',namePt='Inflador da bochecha'},
    {code='AU14',nameEn='Dimpler',namePt='Formador de covinha'},
    {code='AU15',nameEn='Lip Corner Depressor',namePt='Abaixador do canto dos labios'},
    {code='AU16',nameEn='Lower Lip Depressor',namePt='Abaixador do labio inferior'},
    {code='AU17',nameEn='Chin Raiser',namePt='Elevador do queixo'},
    {code='AU18',nameEn='Lip Puckerer',namePt='Projecao dos labios'},
    {code='AU19',nameEn='Tongue Show',namePt='Exposicao da lingua'},
    {code='AU20',nameEn='Lip Stretcher',namePt='Estirador dos labios'},
    {code='AU21',nameEn='Neck Tightener',namePt='Tensionador do pescoco'},
    {code='AU22',nameEn='Lip Funneler',namePt='Funilamento dos labios'},
    {code='AU23',nameEn='Lip Tightener',namePt='Apertador dos labios'},
    {code='AU24',nameEn='Lip Pressor',namePt='Pressionador dos labios'},
    {code='AU25',nameEn='Lips Part',namePt='Separacao dos labios'},
    {code='AU26',nameEn='Jaw Drop',namePt='Abertura da mandibula'},
    {code='AU27',nameEn='Mouth Stretch',namePt='Alongamento da boca'},
    {code='AU28',nameEn='Lip Suck',namePt='Sucao dos labios'},
    {code='AU29',nameEn='Jaw Thrust',namePt='Projecao da mandibula'},
    {code='AU30',nameEn='Jaw Sideways',namePt='Deslocamento lateral da mandibula'},
    {code='AU31',nameEn='Jaw Clencher',namePt='Contracao da mandibula'},
    {code='AU32',nameEn='Lip Bite',namePt='Mordida do labio'},
    {code='AU33',nameEn='Blow',namePt='Sopro'},
    {code='AU34',nameEn='Puff',namePt='Inflar as bochechas'},
    {code='AU35',nameEn='Cheek Suck',namePt='Sucao das bochechas'},
    {code='AU36',nameEn='Tongue Bulge',namePt='Protrusao da lingua'},
    {code='AU37',nameEn='Lip Wipe',namePt='Passar a lingua nos labios'},
    {code='AU38',nameEn='Nostril Dilator',namePt='Dilatador das narinas'},
    {code='AU39',nameEn='Nostril Compressor',namePt='Compressor das narinas'},
    {code='AU41',nameEn='Lid Droop',namePt='Queda da palpebra'},
    {code='AU42',nameEn='Slit',namePt='Olhos semicerrados'},
    {code='AU43',nameEn='Eyes Closed',namePt='Olhos fechados'},
    {code='AU44',nameEn='Squint',namePt='Apertar os olhos'},
    {code='AU45',nameEn='Blink',namePt='Piscar'},
    {code='AU46',nameEn='Wink',namePt='Piscar um olho'}
}

local TEMPLATE_DEFINITIONS={
    {
        id='brow_left', nameKey='fre_facs_brow_left', actionUnits={'AU1','AU2','AU4'},
        color=AREA_COLOR, mirrorSign=1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.105,0.275,-0.11},{-0.13,0.305,-0.11},{-0.17,0.322,-0.11},
                {-0.21,0.330,-0.11},{-0.25,0.321,-0.11},{-0.285,0.292,-0.11},
                {-0.30,0.282,-0.11},{-0.275,0.275,-0.11},{-0.235,0.275,-0.11},
                {-0.19,0.265,-0.11},{-0.145,0.260,-0.11},{-0.115,0.265,-0.11}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.12,0.38,-0.09},{-0.16,0.40,-0.09},{-0.21,0.41,-0.09},
                {-0.26,0.40,-0.09},{-0.29,0.37,-0.09}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{-0.21,0.46,-0.09}}},
            {id='controller',role='controller',kind='point',closed=false,points={{-0.20,0.29,-0.12}}}
        }
    },
    {
        id='brow_right', nameKey='fre_facs_brow_right', actionUnits={'AU1','AU2','AU4'},
        color=AREA_COLOR, mirrorSign=-1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {0.105,0.275,-0.11},{0.13,0.305,-0.11},{0.17,0.322,-0.11},
                {0.21,0.330,-0.11},{0.25,0.321,-0.11},{0.285,0.292,-0.11},
                {0.30,0.282,-0.11},{0.275,0.275,-0.11},{0.235,0.275,-0.11},
                {0.19,0.265,-0.11},{0.145,0.260,-0.11},{0.115,0.265,-0.11}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {0.12,0.38,-0.09},{0.16,0.40,-0.09},{0.21,0.41,-0.09},
                {0.26,0.40,-0.09},{0.29,0.37,-0.09}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{0.21,0.46,-0.09}}},
            {id='controller',role='controller',kind='point',closed=false,points={{0.20,0.29,-0.12}}}
        }
    }
}

-- Neutral authoring guides, relative to the placed face origin. These are
-- FACS-related regions, not individual AU deformations or muscle simulations.
local MORE_LEFT_TEMPLATES={
    {
        id='eye_left',nameKey='fre_facs_eye_left',
        actionUnits={'AU5','AU7','AU41','AU42','AU43','AU44','AU45','AU46'},
        color=AREA_COLOR,mirrorSign=1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.105,0.16,-0.11},{-0.12,0.185,-0.11},{-0.16,0.205,-0.11},
                {-0.20,0.21,-0.11},{-0.245,0.198,-0.11},{-0.275,0.175,-0.11},
                {-0.28,0.15,-0.11},{-0.255,0.13,-0.11},{-0.21,0.12,-0.11},
                {-0.165,0.125,-0.11},{-0.13,0.14,-0.11}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.12,0.215,-0.10},{-0.16,0.228,-0.10},{-0.205,0.232,-0.10},
                {-0.25,0.22,-0.10},{-0.27,0.20,-0.10}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{-0.205,0.245,-0.10}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{-0.20,0.17,-0.11}}},
            {id='lower_lid_line',role='action_line',kind='polyline',closed=false,points={
                {-0.12,0.16,-0.10},{-0.15,0.137,-0.10},{-0.19,0.13,-0.10},
                {-0.23,0.135,-0.10},{-0.26,0.15,-0.10},{-0.275,0.17,-0.10}
            }}
        }
    },
    {
        id='cheek_left',nameKey='fre_facs_cheek_left',actionUnits={'AU6'},
        color=AREA_COLOR,mirrorSign=1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.105,0.055,-0.14},{-0.12,0.09,-0.14},{-0.16,0.11,-0.14},
                {-0.215,0.115,-0.14},{-0.27,0.095,-0.14},{-0.31,0.05,-0.14},
                {-0.30,-0.025,-0.14},{-0.25,-0.075,-0.14},{-0.18,-0.07,-0.14},
                {-0.12,-0.02,-0.14}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.12,0.09,-0.13},{-0.16,0.08,-0.13},{-0.21,0.075,-0.13},
                {-0.265,0.08,-0.13},{-0.30,0.10,-0.13}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{-0.22,0.035,-0.13}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{-0.21,-0.015,-0.14}}}
        }
    },
    {
        id='mouth_corner_left',nameKey='fre_facs_mouth_corner_left',actionUnits={'AU12','AU15'},
        color=AREA_COLOR,mirrorSign=1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.08,-0.18,-0.08},{-0.10,-0.14,-0.08},{-0.14,-0.125,-0.08},
                {-0.19,-0.135,-0.08},{-0.225,-0.17,-0.08},{-0.23,-0.21,-0.08},
                {-0.205,-0.25,-0.08},{-0.165,-0.27,-0.08},{-0.115,-0.25,-0.08},
                {-0.085,-0.22,-0.08}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.11,-0.035,-0.07},{-0.14,-0.065,-0.07},{-0.17,-0.10,-0.07},
                {-0.185,-0.14,-0.07},{-0.19,-0.17,-0.07}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{-0.205,-0.085,-0.07}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{-0.16,-0.20,-0.08}}}
        }
    },
    {
        id='nasolabial_left',nameKey='fre_facs_nasolabial_left',actionUnits={'AU11'},
        color=AREA_COLOR,mirrorSign=1,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.08,0.045,-0.10},{-0.105,0.015,-0.10},{-0.115,-0.03,-0.10},
                {-0.135,-0.08,-0.10},{-0.165,-0.13,-0.10},{-0.195,-0.17,-0.10},
                {-0.205,-0.19,-0.10},{-0.18,-0.195,-0.10},{-0.15,-0.16,-0.10},
                {-0.125,-0.11,-0.10},{-0.105,-0.055,-0.10},{-0.09,-0.005,-0.10}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.085,0.04,-0.10},{-0.105,-0.005,-0.10},{-0.125,-0.05,-0.10},
                {-0.15,-0.105,-0.10},{-0.185,-0.16,-0.10}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{-0.085,-0.015,-0.10}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{-0.15,-0.105,-0.10}}}
        }
    }
}

local function mirroredTemplate(left)
    local right={id=left.id:gsub('_left$','_right'),
        nameKey=left.nameKey:gsub('_left$','_right'),actionUnits={},
        color=AREA_COLOR,mirrorSign=-1,shapes={}}
    for _,actionUnit in ipairs(left.actionUnits) do
        right.actionUnits[#right.actionUnits+1]=actionUnit
    end
    for _,shape in ipairs(left.shapes) do
        local mirrored={id=shape.id,role=shape.role,kind=shape.kind,
            closed=shape.closed,points={}}
        for _,point in ipairs(shape.points) do
            mirrored.points[#mirrored.points+1]={-point[1],point[2],point[3]}
        end
        right.shapes[#right.shapes+1]=mirrored
    end
    return right
end

for _,left in ipairs(MORE_LEFT_TEMPLATES) do
    TEMPLATE_DEFINITIONS[#TEMPLATE_DEFINITIONS+1]=left
    TEMPLATE_DEFINITIONS[#TEMPLATE_DEFINITIONS+1]=mirroredTemplate(left)
end

local LIP_TEMPLATES={
    {
        id='upper_lip',nameKey='fre_facs_upper_lip',actionUnits={'AU10'},color=AREA_COLOR,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.17,-0.18,-0.08},{-0.145,-0.155,-0.08},{-0.11,-0.14,-0.08},
                {-0.075,-0.137,-0.08},{-0.045,-0.15,-0.08},{0,-0.162,-0.08},
                {0.045,-0.15,-0.08},{0.075,-0.137,-0.08},{0.11,-0.14,-0.08},
                {0.145,-0.155,-0.08},{0.17,-0.18,-0.08},{0.14,-0.19,-0.08},
                {0.09,-0.195,-0.08},{0,-0.19,-0.08},{-0.09,-0.195,-0.08},
                {-0.14,-0.19,-0.08}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.17,-0.18,-0.08},{-0.14,-0.155,-0.08},{-0.10,-0.142,-0.08},
                {-0.055,-0.15,-0.08},{0,-0.165,-0.08},{0.055,-0.15,-0.08},
                {0.10,-0.142,-0.08},{0.14,-0.155,-0.08},{0.17,-0.18,-0.08}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{0,-0.105,-0.08}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{0,-0.175,-0.08}}}
        }
    },
    {
        id='lower_lip',nameKey='fre_facs_lower_lip',actionUnits={'AU16'},color=AREA_COLOR,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.17,-0.18,-0.08},{-0.14,-0.19,-0.08},{-0.09,-0.195,-0.08},
                {0,-0.19,-0.08},{0.09,-0.195,-0.08},{0.14,-0.19,-0.08},
                {0.17,-0.18,-0.08},{0.15,-0.21,-0.08},{0.12,-0.23,-0.08},
                {0.07,-0.245,-0.08},{0,-0.25,-0.08},{-0.07,-0.245,-0.08},
                {-0.12,-0.23,-0.08},{-0.15,-0.21,-0.08}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.17,-0.18,-0.08},{-0.14,-0.205,-0.08},{-0.10,-0.23,-0.08},
                {-0.05,-0.245,-0.08},{0,-0.25,-0.08},{0.05,-0.245,-0.08},
                {0.10,-0.23,-0.08},{0.14,-0.205,-0.08},{0.17,-0.18,-0.08}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{0,-0.30,-0.08}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{0,-0.22,-0.08}}}
        }
    }
}
for _,definition in ipairs(LIP_TEMPLATES) do
    TEMPLATE_DEFINITIONS[#TEMPLATE_DEFINITIONS+1]=definition
end

local ANATOMICAL_TEMPLATES={
    {
        id='chin',nameKey='fre_facs_chin',actionUnits={'AU17'},color=AREA_COLOR,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.10,-0.27,-0.09},{-0.06,-0.28,-0.09},{0,-0.29,-0.09},
                {0.06,-0.28,-0.09},{0.10,-0.27,-0.09},{0.14,-0.31,-0.09},
                {0.15,-0.36,-0.09},{0.12,-0.42,-0.09},{0.08,-0.46,-0.09},
                {0,-0.48,-0.09},{-0.08,-0.46,-0.09},{-0.12,-0.42,-0.09},
                {-0.15,-0.36,-0.09},{-0.14,-0.31,-0.09}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.09,-0.275,-0.09},{-0.045,-0.285,-0.09},{0,-0.288,-0.09},
                {0.045,-0.285,-0.09},{0.09,-0.275,-0.09}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{0,-0.245,-0.09}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{0,-0.37,-0.09}}}
        }
    },
    {
        id='jaw',nameKey='fre_facs_jaw',actionUnits={'AU26','AU27','AU29','AU30','AU31'},
        color=AREA_COLOR,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.19,-0.18,-0.09},{-0.25,-0.22,-0.09},{-0.30,-0.28,-0.09},
                {-0.32,-0.34,-0.09},{-0.29,-0.40,-0.09},{-0.23,-0.44,-0.09},
                {-0.15,-0.47,-0.09},{0,-0.49,-0.09},{0.15,-0.47,-0.09},
                {0.23,-0.44,-0.09},{0.29,-0.40,-0.09},{0.32,-0.34,-0.09},
                {0.30,-0.28,-0.09},{0.25,-0.22,-0.09},{0.19,-0.18,-0.09},
                {0.13,-0.23,-0.09},{0,-0.25,-0.09},{-0.13,-0.23,-0.09}
            }},
            {id='action_line',role='action_line',kind='polyline',closed=false,points={
                {-0.30,-0.32,-0.09},{-0.28,-0.38,-0.09},{-0.22,-0.43,-0.09},
                {-0.14,-0.465,-0.09},{0,-0.485,-0.09},{0.14,-0.465,-0.09},
                {0.22,-0.43,-0.09},{0.28,-0.38,-0.09},{0.30,-0.32,-0.09}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{0,-0.53,-0.09}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{0,-0.37,-0.09}}}
        }
    },
    {
        id='nose',nameKey='fre_facs_nose',actionUnits={'AU9','AU38','AU39'},color=AREA_COLOR,
        shapes={
            {id='area',role='area',kind='polygon',closed=true,points={
                {-0.04,0.13,-0.09},{-0.06,0.10,-0.09},{-0.065,0.06,-0.09},
                {-0.075,0.03,-0.09},{-0.10,0.005,-0.09},{-0.095,-0.02,-0.09},
                {-0.075,-0.045,-0.09},{-0.04,-0.055,-0.09},{0,-0.06,-0.09},
                {0.04,-0.055,-0.09},{0.075,-0.045,-0.09},{0.095,-0.02,-0.09},
                {0.10,0.005,-0.09},{0.075,0.03,-0.09},{0.065,0.06,-0.09},
                {0.06,0.10,-0.09},{0.04,0.13,-0.09}
            }},
            {id='bridge_wrinkle',role='action_line',kind='polyline',closed=false,points={
                {-0.045,0.075,-0.09},{-0.025,0.07,-0.09},{0,0.068,-0.09},
                {0.025,0.07,-0.09},{0.045,0.075,-0.09}
            }},
            {id='left_alar_fold',role='action_line',kind='polyline',closed=false,points={
                {-0.045,0.005,-0.09},{-0.065,-0.005,-0.09},{-0.085,-0.015,-0.09}
            }},
            {id='right_alar_fold',role='action_line',kind='polyline',closed=false,points={
                {0.045,0.005,-0.09},{0.065,-0.005,-0.09},{0.085,-0.015,-0.09}
            }},
            {id='line_controller',role='line_controller',kind='point',closed=false,
                points={{0,0.16,-0.09}}},
            {id='controller',role='controller',kind='point',closed=false,
                points={{0,0.015,-0.09}}}
        }
    }
}
for _,definition in ipairs(ANATOMICAL_TEMPLATES) do
    TEMPLATE_DEFINITIONS[#TEMPLATE_DEFINITIONS+1]=definition
end

local function copyColor(color)
    return {r=color.r,g=color.g,b=color.b,a=color.a}
end

function M.displayColor(group,control)
    local groupColor=group and group.color or AREA_COLOR
    local color=control and control.color
    local role=control and control.role
    if role=='area' or not role then return color or groupColor end
    if role=='action_line' then return color or ACTION_LINE_COLOR end
    if role=='controller' then return CONTROLLER_COLOR end
    if role=='line_controller' then return color or LINE_CONTROLLER_COLOR end
    return color or groupColor
end

local function oppositeTemplateId(templateId)
    if type(templateId)~='string' then return nil end
    if templateId:sub(-5)=='_left' then return templateId:sub(1,-6)..'_right' end
    if templateId:sub(-6)=='_right' then return templateId:sub(1,-7)..'_left' end
    return nil
end

-- Mirror only positions. The target keeps its own mesh anchors and authored
-- colors; per-point offsets represent the mirrored world-space positions.
function M.mirrorGroup(project,topology,sourceGroup,centerX)
    if not project or not topology or not sourceGroup then return false end
    centerX=tonumber(centerX) or 0
    local sourceId=sourceGroup.templateId
    local targetId=oppositeTemplateId(sourceId)
    if not targetId then return false end
    local targetGroup
    for _,group in ipairs(project.facialMasks or {}) do
        if group.templateId==targetId then targetGroup=group; break end
    end
    if not targetGroup then return false end
    local targetIds={}
    for _,id in ipairs(targetGroup.controlIds or {}) do targetIds[id]=true end
    local prepared,changed={},false
    local prefix=sourceId..'_'
    for _,sourceControlId in ipairs(sourceGroup.controlIds or {}) do
        if sourceControlId:sub(1,#prefix)==prefix then
            local sourceControl=M.controlById(project,sourceControlId)
            local oppositeId=targetId..sourceControlId:sub(#sourceId+1)
            local targetControl=targetIds[oppositeId] and M.controlById(project,oppositeId) or nil
            if sourceControl and targetControl then
                if #sourceControl.points~=#targetControl.points then return false end
                for index=1,#sourceControl.points do
                    local source=tPointGizmo.position(sourceControl,index,topology)
                    local targetAnchor=targetControl.points[index]
                    local base=targetAnchor and topology.vertices[targetAnchor.index]
                    if not source or not base then return false end
                    local offset={x=2*centerX-source.x-base.x,y=source.y-base.y,
                        z=source.z-base.z-(targetControl.depthOffset or 0)}
                    local old=targetAnchor.offset or {}
                    if math.abs(offset.x-(old.x or 0))>1e-6 or
                            math.abs(offset.y-(old.y or 0))>1e-6 or
                            math.abs(offset.z-(old.z or 0))>1e-6 then changed=true end
                    prepared[#prepared+1]={control=targetControl,index=index,offset=offset}
                end
            end
        end
    end
    if changed then
        for _,item in ipairs(prepared) do
            item.control.points[item.index].offset=item.offset
            item.control.topologySignature=topology.signature or ''
        end
    end
    return changed
end

local function nearestVertex(topology,target,used,faceSubset)
    local bestIndex,bestDistance=nil,math.huge
    for index,vertex in ipairs(topology.vertices or {}) do
        local ref=topology.refs and topology.refs[index]
        if not used[index] and (not faceSubset or ref and ref.subset==faceSubset) then
            local dx,dy,dz=vertex.x-target.x,vertex.y-target.y,vertex.z-target.z
            -- The template defines a front-facing outline. Prioritize its X/Y
            -- shape over guessed depth, so cheek/eyelid vertices cannot pull
            -- the lower brow contour down when the mesh surface curves.
            local distance=dx*dx+dy*dy+dz*dz*0.04
            if distance<bestDistance then bestIndex,bestDistance=index,distance end
        end
    end
    return bestIndex
end

local function worldPoint(origin,scale,point)
    return {x=origin.x+point[1]*scale,y=origin.y+point[2]*scale,z=origin.z+point[3]*scale}
end

local function fitPoints(control,shape,topology,faceOrigin,scale)
    local originRef=topology.refs and topology.refs[faceOrigin.vertexIndex]
    local faceSubset=originRef and originRef.subset
    local used={}
    local points={}
    for _,templatePoint in ipairs(shape.points) do
        local target=worldPoint(faceOrigin.position,scale,templatePoint)
        local index=nearestVertex(topology,target,used,faceSubset)
        if index then
            used[index]=true
            local anchor={index=index}
            if shape.role=='line_controller' then
                local vertex=topology.vertices[index]
                anchor.offset={x=target.x-vertex.x,y=target.y-vertex.y,z=0}
            end
            points[#points+1]=anchor
        end
    end
    control.points=points
    control.topologySignature=topology.signature or ''
end

local function addRelationship(controller,targetId)
    controller.relationships=controller.relationships or {}
    for _,relation in ipairs(controller.relationships) do
        if relation.controlId==targetId then return false end
    end
    controller.relationships[#controller.relationships+1]={controlId=targetId,weight=1}
    return true
end

local function linkControllers(project,group,onlyNew)
    local controls={}
    local changed=false
    for _,controlId in ipairs(group.controlIds or {}) do
        controls[controlId]=M.controlById(project,controlId)
    end
    local prefix=group.templateId and group.templateId~='' and group.templateId or group.id
    local groupController=controls[prefix..'_controller']
    local lineController=controls[prefix..'_line_controller']
    for _,controlId in ipairs(group.controlIds or {}) do
        local target=controls[controlId]
        if groupController and target and target~=groupController and
                (not onlyNew or target==lineController) then
            if addRelationship(groupController,controlId) then changed=true end
        end
        if lineController and target and target.role=='action_line' then
            if addRelationship(lineController,controlId) then changed=true end
        end
    end
    return changed
end

function M.templates()
    return TEMPLATE_DEFINITIONS
end

function M.actionUnitCatalog()
    local templateCodes={}
    for _,definition in ipairs(TEMPLATE_DEFINITIONS) do
        for _,actionUnit in ipairs(definition.actionUnits or {}) do
            templateCodes[actionUnit]=true
        end
    end
    local result={}
    for _,entry in ipairs(FACS_ACTION_UNIT_CATALOG) do
        result[#result+1]={code=entry.code,nameEn=entry.nameEn,namePt=entry.namePt,
            templateAvailable=templateCodes[entry.code]==true}
    end
    return result
end

function M.actionUnits()
    local result={}
    for _,entry in ipairs(FACS_ACTION_UNIT_CATALOG) do result[#result+1]=entry.code end
    return result
end

function M.names(groups)
    local result={}
    for _,group in ipairs(groups or {}) do result[#result+1]=group.name or group.id or '' end
    return result
end

function M.instantiate(project,topology,faceOrigin,scale)
    if type(project)~='table' or type(topology)~='table' or type(faceOrigin)~='table' or
            not faceOrigin.placed or #(project.facialMasks or {})>0 or
            #(topology.vertices or {})==0 then return false end
    project.facialMasks=project.facialMasks or {}
    project.surfaceControls=project.surfaceControls or {}
    local actualScale=math.max(tonumber(scale) or tonumber(faceOrigin.scale) or 1,1e-6)
    for _,definition in ipairs(TEMPLATE_DEFINITIONS) do
        local group={id=definition.id,name=definition.id,nameKey=definition.nameKey,
            templateId=definition.id,actionUnits={},color=copyColor(definition.color),
            controlIds={},enabled=true}
        for _,actionUnit in ipairs(definition.actionUnits) do
            group.actionUnits[#group.actionUnits+1]=actionUnit
        end
        for _,shape in ipairs(definition.shapes) do
            local control={id=definition.id..'_'..shape.id,name=definition.id..'_'..shape.id,
                groupId=definition.id,role=shape.role,kind=shape.kind,closed=shape.closed==true,
                points={},topologySignature=topology.signature or '',depthOffset=0,
                color=copyColor(M.displayColor(group,{role=shape.role})),relationships={},enabled=true}
            fitPoints(control,shape,topology,faceOrigin,actualScale)
            project.surfaceControls[#project.surfaceControls+1]=control
            group.controlIds[#group.controlIds+1]=control.id
        end
        linkControllers(project,group)
        project.facialMasks[#project.facialMasks+1]=group
    end
    return true
end

-- Refit only an explicitly selected stock template. Callers should take an
-- undo snapshot before using this on an already authored project.
function M.refit(project,topology,faceOrigin,scale,group)
    if type(project)~='table' or type(topology)~='table' or type(faceOrigin)~='table' or
            not faceOrigin.placed or type(group)~='table' or #(topology.vertices or {})==0 then return false end
    local definition
    for _,candidate in ipairs(TEMPLATE_DEFINITIONS) do
        if candidate.id==group.templateId then definition=candidate break end
    end
    if not definition then return false end
    local controls={}
    for _,shape in ipairs(definition.shapes) do
        local id=definition.id..'_'..shape.id
        local control=M.controlById(project,id)
        if control then
            if control.groupId~=group.id then return false end
        else
            return false
        end
        controls[#controls+1]={control=control,shape=shape}
    end
    local actualScale=math.max(tonumber(scale) or tonumber(faceOrigin.scale) or 1,1e-6)
    for _,entry in ipairs(controls) do
        fitPoints(entry.control,entry.shape,topology,faceOrigin,actualScale)
    end
    group.actionUnits={}
    for _,actionUnit in ipairs(definition.actionUnits) do
        group.actionUnits[#group.actionUnits+1]=actionUnit
    end
    linkControllers(project,group)
    return true
end

function M.controlById(project,controlId)
    for index,control in ipairs(project and project.surfaceControls or {}) do
        if control.id==controlId then return control,index end
    end
    return nil
end

function M.groupControls(project,group)
    local result={}
    for _,controlId in ipairs(group and group.controlIds or {}) do
        local control,index=M.controlById(project,controlId)
        if control then result[#result+1]={control=control,index=index} end
    end
    return result
end

return M
