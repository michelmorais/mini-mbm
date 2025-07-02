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

#include "physics-box-2d-joint-lua.h"
#include "../plugin-helper/plugin-helper.h"
#include <box2d/box2d.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/class-identifier.h>

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace mbm
{
    b2Joint *getJointBox2dFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<b2Joint **>(plugin_helper::lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_BOX2D_JOINT));
        return *ud;
    }

    void inform_joint_has_no_such_a_method(lua_State *lua, b2Joint* joint, const char* method_name)
    {
        auto type_joint_as_const_char = [] (b2Joint* joint) -> const char * 
        {
            if(joint == nullptr)
                return "null";
            switch (joint->GetType())
            {
                case e_unknownJoint:         return "Unknown";
                case e_revoluteJoint:        return "Revolute";
                case e_prismaticJoint:       return "Prismatic";
                case e_distanceJoint:        return "Distance";
                case e_pulleyJoint:          return "Pulley";
                case e_mouseJoint:           return "Mouse";
                case e_gearJoint:            return "Gear";
                case e_wheelJoint:           return "Wheel";
                case e_weldJoint:            return "Weld";
                case e_frictionJoint:        return "Friction";
                case e_ropeJoint:            return "Rope";
                case e_motorJoint:           return "Motor";
                default:                     return "Unknown";
            }
        };

        plugin_helper::lua_print_line(lua,TYPE_LOG_WARN,"joint [%s] has no such a method [%s]",
                    type_joint_as_const_char(joint),
                    method_name ? method_name : "null");
    }

    int onGetReactionForceJointBox2d(lua_State *lua)
    {
        b2Joint *    joint  = getJointBox2dFromRawTable(lua, 1, 1);
        const float  delta  = luaL_checknumber(lua, 2);
        const b2Vec2 b      = joint->GetReactionForce(delta);
        lua_pushnumber(lua, b.x);
        lua_pushnumber(lua, b.y);
        return 2;
    }

    int onGetReactionTorqueJointBox2d(lua_State *lua)
    {
        b2Joint *   joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float delta = luaL_checknumber(lua, 2);
        const float ret   = joint->GetReactionTorque(delta);
        lua_pushnumber(lua, ret);
        return 1;
    }

    int onIsActiveJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        if (joint->IsEnabled())
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onSetActiveJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const bool activate = lua_toboolean(lua, 2) ? true : false;
        b2Body* body_a = joint->GetBodyA();
        b2Body* body_b = joint->GetBodyB();
        body_a->SetEnabled(activate);
        body_b->SetEnabled(activate);
        return 0;
    }

    int onSetMotorSpeedJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  v = luaL_checknumber(lua, 2);
        switch(joint->GetType())
        {
            case e_revoluteJoint  :
            {
                auto* pJoint = static_cast<b2RevoluteJoint*>(joint);
                if(pJoint)
                    pJoint->SetMotorSpeed(v);
            }
            break;
            case e_prismaticJoint :
            {
                auto* pJoint = static_cast<b2PrismaticJoint*>(joint);
                if(pJoint)
                    pJoint->SetMotorSpeed(v);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    pJoint->SetMotorSpeed(v);
            }
            break;
            default:{inform_joint_has_no_such_a_method(lua,joint,"setMotorSpeed");}
            break;
        }
        return 0;
    }

    int onGetMotorSpeedJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        switch(joint->GetType())
        {
            case e_revoluteJoint  :
            {
                auto* pJoint = static_cast<b2RevoluteJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMotorSpeed());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_prismaticJoint :
            {
                auto* pJoint = static_cast<b2PrismaticJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMotorSpeed());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMotorSpeed());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"getMotorSpeed");
                lua_pushnumber(lua,0.0f);
            }
            break;
        }
        return 1;
    }

    int onSetMaxMotorTorqueJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  v = luaL_checknumber(lua, 2);
        switch(joint->GetType())
        {
            case e_revoluteJoint  :
            {
                auto* pJoint = static_cast<b2RevoluteJoint*>(joint);
                if(pJoint)
                    pJoint->SetMaxMotorTorque(v);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    pJoint->SetMaxMotorTorque(v);
            }
            break;
            case e_motorJoint     :
            {
                auto* pJoint = static_cast<b2MotorJoint*>(joint);
                if(pJoint)
                    pJoint->SetMaxTorque(v);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"setMaxMotorTorque");
            }
            break;
        }
        return 0;
    }

    int onGetMaxMotorTorqueJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        switch(joint->GetType())
        {
            case e_revoluteJoint  :
            {
                auto* pJoint = static_cast<b2RevoluteJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMaxMotorTorque());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMaxMotorTorque());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_motorJoint     :
            {
                auto* pJoint = static_cast<b2MotorJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMaxTorque());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"getMaxMotorTorque");
                lua_pushnumber(lua,0.0f);
            }
            break;
        }
        return 1;
    }

    int onGetRatioJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        switch(joint->GetType())
        {
            case e_distanceJoint  :
            {
                auto* pJoint = static_cast<b2DistanceJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetDamping());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_gearJoint      :
            {
                auto* pJoint = static_cast<b2GearJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetRatio());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetDamping());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_weldJoint      :
            {
                auto* pJoint = static_cast<b2WeldJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetDamping());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"getDamping");
                lua_pushnumber(lua,0.0f);
            }
            break;
        }
        return 1;
    }

    int onSetRatioJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  v = luaL_checknumber(lua, 2);
        switch(joint->GetType())
        {
            case e_distanceJoint  :
            {
                auto* pJoint = static_cast<b2DistanceJoint*>(joint);
                if(pJoint)
                    pJoint->SetDamping(v);
            }
            break;
            case e_gearJoint      :
            {
                auto* pJoint = static_cast<b2GearJoint*>(joint);
                if(pJoint)
                    pJoint->SetRatio(v);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    pJoint->SetDamping(v);
            }
            break;
            case e_weldJoint      :
            {
                auto* pJoint = static_cast<b2WeldJoint*>(joint);
                if(pJoint)
                    pJoint->SetDamping(v);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"setDamping");
            }
            break;
        }
        return 0;
    }

    int onSetMaxForceJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  v = luaL_checknumber(lua, 2);
        switch(joint->GetType())
        {
            case e_mouseJoint     :
            {
                auto* pJoint = static_cast<b2MotorJoint*>(joint);
                if(pJoint)
                    pJoint->SetMaxForce(v);
            }
            break;
            case e_frictionJoint  :
            {
                auto* pJoint = static_cast<b2FrictionJoint*>(joint);
                if(pJoint)
                    pJoint->SetMaxForce(v);
            }
            break;
            case e_motorJoint     :
            {
                auto* pJoint = static_cast<b2MotorJoint*>(joint);
                if(pJoint)
                    pJoint->SetMaxForce(v);
            }
            break;
            case e_prismaticJoint     :
            {
                auto* pJoint = static_cast<b2PrismaticJoint*>(joint);
                if(pJoint)
                    pJoint->SetMaxMotorForce(v);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"setMaxForce");
            }
            break;
        }
        return 0;
    }

    int onGetMaxForceJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        switch(joint->GetType())
        {
            case e_mouseJoint     :
            {
                auto* pJoint = static_cast<b2MotorJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMaxForce());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_frictionJoint  :
            {
                auto* pJoint = static_cast<b2FrictionJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMaxForce());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_motorJoint     :
            {
                auto* pJoint = static_cast<b2MotorJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMaxForce());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_prismaticJoint     :
            {
                auto* pJoint = static_cast<b2PrismaticJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetMaxMotorForce());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"getMaxForce");
                lua_pushnumber(lua,0.0f);
            }
            break;
        }
        return 1;
    }

    int onSetFrequencyHzJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  v = luaL_checknumber(lua, 2);
        switch(joint->GetType())
        {
            case e_distanceJoint  :
            {
                auto* pJoint = static_cast<b2DistanceJoint*>(joint);
                if(pJoint)
                    pJoint->SetStiffness(v);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    pJoint->SetStiffness(v);
            }
            break;
            case e_weldJoint      :
            {
                auto* pJoint = static_cast<b2WeldJoint*>(joint);
                if(pJoint)
                    pJoint->SetStiffness(v);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"SetStiffness");
            }
            break;
        }
        return 0;
    }

    int onGetFrequencyHzJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        switch(joint->GetType())
        {
            case e_distanceJoint  :
            {
                auto* pJoint = static_cast<b2DistanceJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetStiffness());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetStiffness());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            case e_weldJoint      :
            {
                auto* pJoint = static_cast<b2WeldJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetStiffness());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"getStiffness");
                lua_pushnumber(lua,0.0f);
            }
            break;
        }
        return 1;
    }

    int onSetLengthJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  v = luaL_checknumber(lua, 2);
        switch(joint->GetType())
        {
            case e_distanceJoint  :
            {
                auto* pJoint = static_cast<b2DistanceJoint*>(joint);
                if(pJoint)
                    pJoint->SetLength(v);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"setLength");
            }
            break;
        }
        return 0;
    }

    int onGetLengthJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        switch(joint->GetType())
        {
            case e_distanceJoint  :
            {
                auto* pJoint = static_cast<b2DistanceJoint*>(joint);
                if(pJoint)
                    lua_pushnumber(lua,pJoint->GetLength());
                else
                    lua_pushnumber(lua,0.0f);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"getLength");
                lua_pushnumber(lua,0.0f);
            }
            break;
        }
        return 1;
    }

    int onSetTargetJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  x = luaL_checknumber(lua, 2);
        const float  y = luaL_checknumber(lua, 3);
        const b2Vec2 v(x,y);
        switch(joint->GetType())
        {
            case e_mouseJoint:
            {
                auto* pJoint = static_cast<b2MouseJoint*>(joint);
                if(pJoint)
                    pJoint->SetTarget(v);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"setTarget");
            }
            break;
        }
        return 0;
    }

    int onGetTargetJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        switch(joint->GetType())
        {
            case e_mouseJoint:
            {
                auto* pJoint = static_cast<b2MouseJoint*>(joint);
                if(pJoint)
                {
                    const b2Vec2 v = pJoint->GetTarget();
                    lua_pushnumber(lua,v.x);
                    lua_pushnumber(lua,v.y);
                }
                else
                {
                    lua_pushnumber(lua,0.0f);
                    lua_pushnumber(lua,0.0f);
                }
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"getTarget");
                lua_pushnumber(lua,0.0f);
                lua_pushnumber(lua,0.0f);
            }
            break;
        }
        return 2;
    }

    int onGetAnchorA(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const b2Vec2 anchorA(joint->GetAnchorA());
        lua_pushnumber(lua,anchorA.x);
        lua_pushnumber(lua,anchorA.y);
        return 2;
    }

    int onGetAnchorB(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const b2Vec2 anchorB(joint->GetAnchorB());
        lua_pushnumber(lua,anchorB.x);
        lua_pushnumber(lua,anchorB.y);
        return 2;
    }

    int onEnableMotorJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const bool  v = lua_toboolean(lua, 2) ? true : false;
        switch(joint->GetType())
        {
            case e_revoluteJoint  :
            {
                auto* pJoint = static_cast<b2RevoluteJoint*>(joint);
                if(pJoint)
                    pJoint->EnableMotor(v);
            }
            break;
            case e_prismaticJoint :
            {
                auto* pJoint = static_cast<b2PrismaticJoint*>(joint);
                if(pJoint)
                    pJoint->EnableMotor(v);
            }
            break;
            case e_wheelJoint     :
            {
                auto* pJoint = static_cast<b2WheelJoint*>(joint);
                if(pJoint)
                    pJoint->EnableMotor(v);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"enableMotor");
            }
            break;
        }
        return 0;
    }

    int onSetCorrectionFactorJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  v = luaL_checknumber(lua, 2);
        if(joint->GetType() == e_motorJoint)
        {
            auto* pJoint = static_cast<b2MotorJoint*>(joint);
            if(pJoint)
                pJoint->SetCorrectionFactor(v);
        }
        else
        {
            inform_joint_has_no_such_a_method(lua,joint,"setCorrectionFactor");
        }
        return 0;
    }

    int onGetCorrectionFactorJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        float  v = 0.0f;
        if(joint->GetType() == e_motorJoint)
        {
            auto* pJoint = static_cast<b2MotorJoint*>(joint);
            if(pJoint)
                v = pJoint->GetCorrectionFactor();
        }
        else
        {
            inform_joint_has_no_such_a_method(lua,joint,"getCorrectionFactor");
        }
        lua_pushnumber(lua,v);
        return 1;
    }

    int onSetLinearOffsetJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  x = luaL_checknumber(lua, 2);
        const float  y = luaL_checknumber(lua, 3);
        if(joint->GetType() == e_motorJoint)
        {
            auto* pJoint = static_cast<b2MotorJoint*>(joint);
            if(pJoint)
            {
                const b2Vec2 v(x,y);
                pJoint->SetLinearOffset(v);
            }
        }
        else
        {
            inform_joint_has_no_such_a_method(lua,joint,"setLinearOffset");
        }
        return 0;
    }

    int onGetLinearOffsetJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        b2Vec2  v(0,0);
        if(joint->GetType() == e_motorJoint)
        {
            auto* pJoint = static_cast<b2MotorJoint*>(joint);
            if(pJoint)
                v = pJoint->GetLinearOffset();
        }
        else
        {
            inform_joint_has_no_such_a_method(lua,joint,"getLinearOffset");
        }
        lua_pushnumber(lua,v.x);
        lua_pushnumber(lua,v.y);
        return 2;
    }

    int onSetAngularOffsetJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  x = luaL_checknumber(lua, 2);
        if(joint->GetType() == e_motorJoint)
        {
            auto* pJoint = static_cast<b2MotorJoint*>(joint);
            if(pJoint)
                pJoint->SetAngularOffset(x);
        }
        else
        {
            inform_joint_has_no_such_a_method(lua,joint,"setAngularOffset");
        }
        return 0;
    }

    int onGetAngularOffsetJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        float  v = 0.0f;
        if(joint->GetType() == e_motorJoint)
        {
            auto* pJoint = static_cast<b2MotorJoint*>(joint);
            if(pJoint)
                v = pJoint->GetAngularOffset();
        }
        else
        {
            inform_joint_has_no_such_a_method(lua,joint,"getAngularOffset");
        }
        lua_pushnumber(lua,v);
        return 1;
    }

    int onEnableLimitJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const bool  v = lua_toboolean(lua, 2) ? true : false;
        switch(joint->GetType())
        {
            case e_revoluteJoint  :
            {
                auto* pJoint = static_cast<b2RevoluteJoint*>(joint);
                if(pJoint)
                    pJoint->EnableLimit(v);
            }
            break;
            case e_prismaticJoint :
            {
                auto* pJoint = static_cast<b2PrismaticJoint*>(joint);
                if(pJoint)
                    pJoint->EnableLimit(v);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"enableLimit");
            }
            break;
        }
        return 0;
    }

    int onSetLimitsJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        const float  lower = luaL_checknumber(lua, 2);
        const float  upper = luaL_checknumber(lua, 3);
        switch(joint->GetType())
        {
            case e_revoluteJoint  :
            {
                auto* pJoint = static_cast<b2RevoluteJoint*>(joint);
                if(pJoint)
                    pJoint->SetLimits(lower,upper);
            }
            break;
            case e_prismaticJoint :
            {
                auto* pJoint = static_cast<b2PrismaticJoint*>(joint);
                if(pJoint)
                    pJoint->SetLimits(lower,upper);
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"setLimits");
            }
            break;
        }
        return 0;
    }

    int onGetLimitsJointBox2d(lua_State *lua)
    {
        b2Joint *joint = getJointBox2dFromRawTable(lua, 1, 1);
        float lower = 0.0f;
        float upper = 0.0f;
        switch(joint->GetType())
        {
            case e_revoluteJoint  :
            {
                auto* pJoint = static_cast<b2RevoluteJoint*>(joint);
                if(pJoint)
                {
                    lower = pJoint->GetLowerLimit();
                    upper = pJoint->GetUpperLimit();
                }
            }
            break;
            case e_prismaticJoint :
            {
                auto* pJoint = static_cast<b2PrismaticJoint*>(joint);
                if(pJoint)
                {
                    lower = pJoint->GetLowerLimit();
                    upper = pJoint->GetUpperLimit();
                }
            }
            break;
            default:
            {
                inform_joint_has_no_such_a_method(lua,joint,"getLimits");
            }
            break;
        }
        lua_pushnumber(lua,lower);
        lua_pushnumber(lua,upper);
        return 2;
    }

    int onGetJointLua(lua_State *lua, b2Joint *joint)
    {
        lua_settop(lua, 0);
        luaL_Reg regMethods[] = {   {"getReactionForce", onGetReactionForceJointBox2d},
                                    {"getReactionTorque", onGetReactionTorqueJointBox2d},
                                    {"isActive", onIsActiveJointBox2d},
                                    {"setActive", onSetActiveJointBox2d},

                                    {"getMotorSpeed", onGetMotorSpeedJointBox2d},
                                    {"setMotorSpeed", onSetMotorSpeedJointBox2d},
                                    
                                    {"getMaxMotorTorque", onGetMaxMotorTorqueJointBox2d},
                                    {"setMaxMotorTorque", onSetMaxMotorTorqueJointBox2d},
                                    
                                    {"getDamping", onGetRatioJointBox2d},
                                    {"setDamping", onSetRatioJointBox2d},

                                    {"setMaxForce", onSetMaxForceJointBox2d},
                                    {"getMaxForce", onGetMaxForceJointBox2d},

                                    {"setStiffness", onSetFrequencyHzJointBox2d},
                                    {"getStiffness", onGetFrequencyHzJointBox2d},

                                    {"getLength", onGetLengthJointBox2d},
                                    {"setLength", onSetLengthJointBox2d},

                                    {"getTarget", onGetTargetJointBox2d},
                                    {"setTarget", onSetTargetJointBox2d},

                                    {"enableMotor", onEnableMotorJointBox2d},
                                    {"enableLimit", onEnableLimitJointBox2d},
            
                                    {"getCorrectionFactor", onGetCorrectionFactorJointBox2d},
                                    {"setCorrectionFactor", onSetCorrectionFactorJointBox2d},

                                    {"getLinearOffset", onGetLinearOffsetJointBox2d},
                                    {"setLinearOffset", onSetLinearOffsetJointBox2d},

                                    {"getAngularOffset", onGetAngularOffsetJointBox2d},
                                    {"setAngularOffset", onSetAngularOffsetJointBox2d},

                                    {"getLimits", onGetLimitsJointBox2d},
                                    {"setLimits", onSetLimitsJointBox2d},

                                    {"getAnchorA", onGetAnchorA},
                                    {"getAnchorB", onGetAnchorB},
                                    
                                    {nullptr, nullptr}};
        luaL_newlib(lua, regMethods);
        auto **udata = static_cast<b2Joint **>(lua_newuserdata(lua, sizeof(b2Joint *)));
        *udata          = joint;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_BOX2D_JOINT);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }
};
