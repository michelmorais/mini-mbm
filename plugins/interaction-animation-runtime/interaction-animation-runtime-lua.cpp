/*-----------------------------------------------------------------------------------------------------------------------|
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
|-----------------------------------------------------------------------------------------------------------------------*/

#include "interaction-animation-runtime-lua.h"

#include <plugin-helper/plugin-helper.h>
#include <core_mbm/util-interface.h>
#include <render/mesh.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

constexpr const char *INSTANCE_METATABLE = "_mbmInteractionAnimationRuntime";
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;

int PLUGIN_IDENTIFIER = 1;

struct VEC3D
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct MATRIX3
{
    double value[3][3] = {};
};

struct TRANSFORM
{
    VEC3D position;
    VEC3D angle;
    VEC3D scale{1.0, 1.0, 1.0};
    bool visible = true;
};

enum class ACTION : uint8_t
{
    TRANSFORM,
    PLAY,
    NOTIFY,
    VISIBILITY,
};

struct EVENT
{
    std::string id;
    std::string target;
    std::string clipKey;
    std::string transition = "instant";
    ACTION action = ACTION::NOTIFY;
    double time = 0.0;
    double transitionStart = 0.0;
    double transitionEnd = 0.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    double sx = 1.0;
    double sy = 1.0;
    double sz = 1.0;
    bool visible = true;
};

struct ACTOR_DEFINITION
{
    std::string id;
    std::string name;
    std::string clipKey;
};

struct DEFINITION
{
    std::vector<ACTOR_DEFINITION> actors;
    std::unordered_map<std::string, std::size_t> actorIndex;
    std::vector<EVENT> events;
    std::string anchorActor;
    std::string clockActor;
    std::string clockKey;
    double duration = 0.0;
    bool continueClipsAfterTimeline = false;
};

struct ACTOR_INSTANCE
{
    ACTOR_DEFINITION definition;
    mbm::MESH *mesh = nullptr;
    TRANSFORM base;
    int luaReference = LUA_NOREF;
    std::string activeClip;
};

MATRIX3 identityMatrix()
{
    MATRIX3 result;
    result.value[0][0] = 1.0;
    result.value[1][1] = 1.0;
    result.value[2][2] = 1.0;
    return result;
}

MATRIX3 multiply(const MATRIX3 &a, const MATRIX3 &b)
{
    MATRIX3 result;
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            result.value[row][column] =
                a.value[row][0] * b.value[0][column] +
                a.value[row][1] * b.value[1][column] +
                a.value[row][2] * b.value[2][column];
        }
    }
    return result;
}

MATRIX3 axisRotation(const VEC3D &axis, const double angle)
{
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const double complement = 1.0 - cosine;
    MATRIX3 result;
    result.value[0][0] = complement * axis.x * axis.x + cosine;
    result.value[0][1] = complement * axis.x * axis.y + sine * axis.z;
    result.value[0][2] = complement * axis.x * axis.z - sine * axis.y;
    result.value[1][0] = complement * axis.y * axis.x - sine * axis.z;
    result.value[1][1] = complement * axis.y * axis.y + cosine;
    result.value[1][2] = complement * axis.y * axis.z + sine * axis.x;
    result.value[2][0] = complement * axis.z * axis.x + sine * axis.y;
    result.value[2][1] = complement * axis.z * axis.y - sine * axis.x;
    result.value[2][2] = complement * axis.z * axis.z + cosine;
    return result;
}

MATRIX3 eulerRotation(const VEC3D &angle)
{
    const MATRIX3 xRotation = axisRotation({1.0, 0.0, 0.0}, angle.x);
    const MATRIX3 yRotation = axisRotation({0.0, 1.0, 0.0}, angle.y);
    const MATRIX3 zRotation = axisRotation({0.0, 0.0, 1.0}, angle.z);
    return multiply(zRotation, multiply(yRotation, xRotation));
}

bool isIdentity(const MATRIX3 &matrix)
{
    constexpr double EPSILON = 0.000001;
    return std::abs(matrix.value[0][0] - 1.0) < EPSILON &&
        std::abs(matrix.value[1][1] - 1.0) < EPSILON &&
        std::abs(matrix.value[2][2] - 1.0) < EPSILON &&
        std::abs(matrix.value[0][1]) < EPSILON && std::abs(matrix.value[0][2]) < EPSILON &&
        std::abs(matrix.value[1][0]) < EPSILON && std::abs(matrix.value[1][2]) < EPSILON &&
        std::abs(matrix.value[2][0]) < EPSILON && std::abs(matrix.value[2][1]) < EPSILON;
}

VEC3D rotationToEuler(const MATRIX3 &matrix)
{
    const double value = std::max(-1.0, std::min(1.0, matrix.value[2][0]));
    const double y = std::asin(value);
    double x = 0.0;
    double z = 0.0;
    if (std::abs(matrix.value[2][0]) > 0.999999)
    {
        x = 0.0;
        z = std::atan2(matrix.value[0][1], matrix.value[1][1]);
    }
    else
    {
        x = std::atan2(-matrix.value[2][1], matrix.value[2][2]);
        z = std::atan2(-matrix.value[1][0], matrix.value[0][0]);
    }
    return {x, y, z};
}

VEC3D row(const MATRIX3 &matrix, const int index)
{
    return {matrix.value[index][0], matrix.value[index][1], matrix.value[index][2]};
}

MATRIX3 anchorRotation(const std::array<VEC3D, 3> &axes, const EVENT &event,
                       const double progress)
{
    const MATRIX3 xRotation = axisRotation(axes[0], event.rx * DEG_TO_RAD * progress);
    const MATRIX3 yRotation = axisRotation(axes[1], event.ry * DEG_TO_RAD * progress);
    const MATRIX3 zRotation = axisRotation(axes[2], event.rz * DEG_TO_RAD * progress);
    return multiply(zRotation, multiply(yRotation, xRotation));
}

double clamp(const double value, const double minimum, const double maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

double transformProgress(const EVENT &event, const double time)
{
    if (event.transition != "smooth") return 1.0;
    if (time < event.transitionStart) return 0.0;
    if (event.transitionEnd <= event.transitionStart)
        return time >= event.transitionEnd ? 1.0 : 0.0;
    const double progress = clamp(
        (time - event.transitionStart) / (event.transitionEnd - event.transitionStart), 0.0, 1.0);
    return progress * progress * (3.0 - 2.0 * progress);
}

double eventTriggerTime(const EVENT &event)
{
    return event.action == ACTION::TRANSFORM && event.transition == "smooth"
        ? event.transitionStart : event.time;
}

const char *actionName(const ACTION action)
{
    switch (action)
    {
        case ACTION::TRANSFORM: return "transform";
        case ACTION::PLAY: return "play";
        case ACTION::NOTIFY: return "notify";
        case ACTION::VISIBILITY: return "visibility";
    }
    return "unknown";
}

void setTransform(mbm::MESH *mesh, const TRANSFORM &transform)
{
    mesh->setPosition({static_cast<float>(transform.position.x),
                       static_cast<float>(transform.position.y),
                       static_cast<float>(transform.position.z)});
    mesh->setAngle({static_cast<float>(transform.angle.x),
                    static_cast<float>(transform.angle.y),
                    static_cast<float>(transform.angle.z)});
    mesh->setScale({static_cast<float>(transform.scale.x),
                    static_cast<float>(transform.scale.y),
                    static_cast<float>(transform.scale.z)});
    mesh->setEnableRender(transform.visible);
}

TRANSFORM readTransform(const mbm::MESH *mesh)
{
    const mbm::VEC3 &position = mesh->getPosition();
    const mbm::VEC3 &angle = mesh->getAngle();
    const mbm::VEC3 &scale = mesh->getScale();
    return {{position.x, position.y, position.z},
            {angle.x, angle.y, angle.z},
            {scale.x, scale.y, scale.z},
            mesh->isRenderEnabled()};
}

class INTERACTION_INSTANCE
{
  public:
    explicit INTERACTION_INSTANCE(lua_State *lua) : lua(lua) {}
    ~INTERACTION_INSTANCE();

    void update(const double delta);
    void restart();
    void stop();
    bool isFinished() const noexcept { return finished; }

    lua_State *lua = nullptr;
    int weakInstanceReference = LUA_NOREF;
    int onStartReference = LUA_NOREF;
    int onEventReference = LUA_NOREF;
    int onFinishReference = LUA_NOREF;
    DEFINITION definition;
    std::vector<ACTOR_INSTANCE> actors;
    std::unordered_map<std::string, std::size_t> actorIndex;
    std::array<VEC3D, 3> axes{};
    double time = 0.0;
    bool finished = false;
    bool running = true;
    std::vector<bool> fired;
    std::vector<bool> notified;

    void initializeAxes();
    void playClip(ACTOR_INSTANCE &actor, const std::string &key);
    void stopClip(ACTOR_INSTANCE &actor, const std::string &key);
    void pauseClip(ACTOR_INSTANCE &actor, const std::string &key);
    void applyTransforms();
    void applyVisibility();
    void applyPlayEvents(double previousTime, double currentTime, bool includeInitial);
    void notifyEvents(double previousTime, double currentTime, bool includeInitial);
    void resetInstance();
    void finish();
    void callCallback(int reference, const EVENT *event, std::size_t eventIndex);
    void pushInstanceTable();
};

INTERACTION_INSTANCE::~INTERACTION_INSTANCE()
{
    for (ACTOR_INSTANCE &actor : actors)
    {
        if (actor.luaReference != LUA_NOREF)
            luaL_unref(lua, LUA_REGISTRYINDEX, actor.luaReference);
    }
    if (weakInstanceReference != LUA_NOREF)
        luaL_unref(lua, LUA_REGISTRYINDEX, weakInstanceReference);
    if (onStartReference != LUA_NOREF)
        luaL_unref(lua, LUA_REGISTRYINDEX, onStartReference);
    if (onEventReference != LUA_NOREF)
        luaL_unref(lua, LUA_REGISTRYINDEX, onEventReference);
    if (onFinishReference != LUA_NOREF)
        luaL_unref(lua, LUA_REGISTRYINDEX, onFinishReference);
}

void INTERACTION_INSTANCE::initializeAxes()
{
    const std::size_t anchorIndex = actorIndex.at(definition.anchorActor);
    const MATRIX3 matrix = eulerRotation(actors[anchorIndex].base.angle);
    axes[0] = row(matrix, 0);
    axes[1] = row(matrix, 1);
    axes[2] = row(matrix, 2);
}

void INTERACTION_INSTANCE::playClip(ACTOR_INSTANCE &actor, const std::string &key)
{
    const std::size_t separator = key.find(':');
    if (separator == std::string::npos || separator + 1 >= key.size()) return;
    const std::string kind = key.substr(0, separator);
    const char *name = key.c_str() + separator + 1;
    if (kind == "skeletal") actor.mesh->playSkeletalAnimation(name);
    else if (kind == "articulated") actor.mesh->playArticulatedAnimation(name, 0, 0.0f, 1.0f);
    else return;
    actor.activeClip = key;
}

void INTERACTION_INSTANCE::stopClip(ACTOR_INSTANCE &actor, const std::string &key)
{
    const std::size_t separator = key.find(':');
    if (separator == std::string::npos || separator + 1 >= key.size()) return;
    const std::string kind = key.substr(0, separator);
    const char *name = key.c_str() + separator + 1;
    if (kind == "skeletal") actor.mesh->stopSkeletalAnimation();
    else if (kind == "articulated") actor.mesh->disableArticulatedAnimation(name);
}

void INTERACTION_INSTANCE::pauseClip(ACTOR_INSTANCE &actor, const std::string &key)
{
    const std::size_t separator = key.find(':');
    if (separator == std::string::npos || separator + 1 >= key.size()) return;
    const std::string kind = key.substr(0, separator);
    const char *name = key.c_str() + separator + 1;
    if (kind == "skeletal") actor.mesh->pauseSkeletalAnimation();
    else if (kind == "articulated") actor.mesh->pauseArticulatedAnimation(name);
}

void INTERACTION_INSTANCE::applyTransforms()
{
    std::vector<VEC3D> positions(actors.size());
    std::vector<MATRIX3> rotations(actors.size(), identityMatrix());
    std::vector<VEC3D> scales(actors.size(), {1.0, 1.0, 1.0});
    for (const EVENT &event : definition.events)
    {
        if (event.action != ACTION::TRANSFORM) continue;
        const auto found = actorIndex.find(event.target);
        if (found == actorIndex.end()) continue;
        const double progress = transformProgress(event, time);
        if (progress <= 0.0) continue;
        const std::size_t index = found->second;
        positions[index].x += axes[0].x * event.x * progress +
            axes[1].x * event.y * progress + axes[2].x * event.z * progress;
        positions[index].y += axes[0].y * event.x * progress +
            axes[1].y * event.y * progress + axes[2].y * event.z * progress;
        positions[index].z += axes[0].z * event.x * progress +
            axes[1].z * event.y * progress + axes[2].z * event.z * progress;
        rotations[index] = multiply(rotations[index], anchorRotation(axes, event, progress));
        scales[index].x *= 1.0 + (event.sx - 1.0) * progress;
        scales[index].y *= 1.0 + (event.sy - 1.0) * progress;
        scales[index].z *= 1.0 + (event.sz - 1.0) * progress;
    }
    for (std::size_t index = 0; index < actors.size(); ++index)
    {
        ACTOR_INSTANCE &actor = actors[index];
        TRANSFORM result = actor.base;
        result.position.x += positions[index].x;
        result.position.y += positions[index].y;
        result.position.z += positions[index].z;
        result.scale.x *= scales[index].x;
        result.scale.y *= scales[index].y;
        result.scale.z *= scales[index].z;
        if (!isIdentity(rotations[index]))
        {
            result.angle = rotationToEuler(multiply(eulerRotation(actor.base.angle), rotations[index]));
        }
        setTransform(actor.mesh, result);
    }
}

void INTERACTION_INSTANCE::applyVisibility()
{
    for (ACTOR_INSTANCE &actor : actors) actor.mesh->setEnableRender(actor.base.visible);
    for (const EVENT &event : definition.events)
    {
        if (event.action != ACTION::VISIBILITY || event.time > time) continue;
        const auto found = actorIndex.find(event.target);
        if (found != actorIndex.end()) actors[found->second].mesh->setEnableRender(event.visible);
    }
}

void INTERACTION_INSTANCE::applyPlayEvents(const double previousTime, const double currentTime,
                                           const bool includeInitial)
{
    for (std::size_t index = 0; index < definition.events.size(); ++index)
    {
        const EVENT &event = definition.events[index];
        if (event.action != ACTION::PLAY || fired[index]) continue;
        const bool reached = includeInitial ? event.time <= currentTime
            : event.time > previousTime && event.time <= currentTime;
        if (!reached) continue;
        const auto found = actorIndex.find(event.target);
        if (found != actorIndex.end()) playClip(actors[found->second], event.clipKey);
        fired[index] = true;
    }
}

void INTERACTION_INSTANCE::callCallback(const int reference, const EVENT *event,
                                        const std::size_t eventIndex)
{
    if (reference == LUA_NOREF) return;
    lua_rawgeti(lua, LUA_REGISTRYINDEX, reference);
    pushInstanceTable();
    int arguments = 1;
    if (event)
    {
        lua_newtable(lua);
        lua_pushstring(lua, event->id.c_str()); lua_setfield(lua, -2, "id");
        lua_pushnumber(lua, event->time); lua_setfield(lua, -2, "time");
        lua_pushstring(lua, actionName(event->action)); lua_setfield(lua, -2, "action");
        lua_pushstring(lua, event->target.c_str()); lua_setfield(lua, -2, "target");
        lua_pushstring(lua, event->clipKey.c_str()); lua_setfield(lua, -2, "clipKey");
        lua_pushstring(lua, event->transition.c_str()); lua_setfield(lua, -2, "transition");
        lua_pushnumber(lua, event->transitionStart); lua_setfield(lua, -2, "transitionStart");
        lua_pushnumber(lua, event->transitionEnd); lua_setfield(lua, -2, "transitionEnd");
        lua_pushnumber(lua, event->x); lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, event->y); lua_setfield(lua, -2, "y");
        lua_pushnumber(lua, event->z); lua_setfield(lua, -2, "z");
        lua_pushnumber(lua, event->rx); lua_setfield(lua, -2, "rx");
        lua_pushnumber(lua, event->ry); lua_setfield(lua, -2, "ry");
        lua_pushnumber(lua, event->rz); lua_setfield(lua, -2, "rz");
        lua_pushnumber(lua, event->sx); lua_setfield(lua, -2, "sx");
        lua_pushnumber(lua, event->sy); lua_setfield(lua, -2, "sy");
        lua_pushnumber(lua, event->sz); lua_setfield(lua, -2, "sz");
        lua_pushboolean(lua, event->visible); lua_setfield(lua, -2, "visible");
        lua_pushinteger(lua, static_cast<lua_Integer>(eventIndex + 1));
        arguments = 3;
    }
    if (lua_pcall(lua, arguments, 0, 0) != LUA_OK)
    {
        mbm::lua_print_line(lua, TYPE_LOG_ERROR, "interaction_animation_runtime callback failed: %s",
                            lua_tostring(lua, -1));
        lua_pop(lua, 1);
    }
}

void INTERACTION_INSTANCE::pushInstanceTable()
{
    lua_rawgeti(lua, LUA_REGISTRYINDEX, weakInstanceReference);
    lua_pushlightuserdata(lua, this);
    lua_rawget(lua, -2);
    lua_remove(lua, -2);
}

void INTERACTION_INSTANCE::notifyEvents(const double previousTime, const double currentTime,
                                        const bool includeInitial)
{
    for (std::size_t index = 0; index < definition.events.size(); ++index)
    {
        const EVENT &event = definition.events[index];
        if (notified[index]) continue;
        const double triggerTime = eventTriggerTime(event);
        const bool reached = includeInitial ? triggerTime <= currentTime
            : triggerTime > previousTime && triggerTime <= currentTime;
        if (!reached) continue;
        notified[index] = true;
        callCallback(onEventReference, &event, index);
    }
}

void INTERACTION_INSTANCE::resetInstance()
{
    for (ACTOR_INSTANCE &actor : actors)
    {
        stopClip(actor, actor.activeClip.empty() ? actor.definition.clipKey : actor.activeClip);
        actor.activeClip.clear();
        setTransform(actor.mesh, actor.base);
    }
    time = 0.0;
    fired.assign(definition.events.size(), false);
    notified.assign(definition.events.size(), false);
    finished = false;
    running = true;
    for (ACTOR_INSTANCE &actor : actors)
    {
        std::string key = actor.definition.clipKey;
        if (actor.definition.id == definition.clockActor) key = definition.clockKey;
        if (!key.empty()) playClip(actor, key);
    }
    applyPlayEvents(-0.0001, 0.0, true);
    notifyEvents(-0.0001, 0.0, true);
    applyTransforms();
    applyVisibility();
}

void INTERACTION_INSTANCE::finish()
{
    if (finished) return;
    finished = true;
    running = false;
    callCallback(onFinishReference, nullptr, 0);
}

void INTERACTION_INSTANCE::update(const double delta)
{
    if (!running || finished) return;
    const double previousTime = time;
    const double amount = std::max(0.0, delta);
    time = std::min(definition.duration, previousTime + amount);
    applyPlayEvents(previousTime, time, false);
    notifyEvents(previousTime, time, false);
    applyTransforms();
    applyVisibility();
    if (time >= definition.duration)
    {
        if (!definition.continueClipsAfterTimeline)
        {
            for (ACTOR_INSTANCE &actor : actors)
                pauseClip(actor, actor.activeClip.empty() ? actor.definition.clipKey : actor.activeClip);
        }
        finish();
    }
}

void INTERACTION_INSTANCE::restart()
{
    resetInstance();
    callCallback(onStartReference, nullptr, 0);
}

void INTERACTION_INSTANCE::stop()
{
    for (ACTOR_INSTANCE &actor : actors)
    {
        stopClip(actor, actor.activeClip.empty() ? actor.definition.clipKey : actor.activeClip);
        actor.activeClip.clear();
        setTransform(actor.mesh, actor.base);
    }
    finish();
}

double numberField(lua_State *lua, const int table, const char *field, const double fallback)
{
    lua_getfield(lua, table, field);
    if (lua_isnil(lua, -1))
    {
        lua_pop(lua, 1);
        return fallback;
    }
    const double value = luaL_checknumber(lua, -1);
    lua_pop(lua, 1);
    if (!std::isfinite(value)) luaL_error(lua, "Interaction field '%s' must be finite", field);
    return value;
}

std::string stringField(lua_State *lua, const int table, const char *field,
                        const char *fallback = "")
{
    lua_getfield(lua, table, field);
    if (lua_isnil(lua, -1))
    {
        lua_pop(lua, 1);
        return fallback;
    }
    const char *value = luaL_checkstring(lua, -1);
    std::string result(value);
    lua_pop(lua, 1);
    return result;
}

bool booleanField(lua_State *lua, const int table, const char *field, const bool fallback)
{
    lua_getfield(lua, table, field);
    if (lua_isnil(lua, -1))
    {
        lua_pop(lua, 1);
        return fallback;
    }
    const bool result = lua_toboolean(lua, -1) != 0;
    lua_pop(lua, 1);
    return result;
}

std::size_t findActor(const DEFINITION &definition, const std::string &id)
{
    const auto found = definition.actorIndex.find(id);
    return found == definition.actorIndex.end() ? std::numeric_limits<std::size_t>::max() : found->second;
}

void parseEvent(lua_State *lua, const int table, const double duration, EVENT &event)
{
    const std::string action = stringField(lua, table, "action");
    if (action == "transform") event.action = ACTION::TRANSFORM;
    else if (action == "play") event.action = ACTION::PLAY;
    else if (action == "notify") event.action = ACTION::NOTIFY;
    else if (action == "visibility") event.action = ACTION::VISIBILITY;
    else luaL_error(lua, "Unknown interaction event action: %s", action.c_str());
    event.id = stringField(lua, table, "id");
    event.target = stringField(lua, table, "target");
    event.clipKey = stringField(lua, table, "clipKey");
    event.transition = stringField(lua, table, "transition", "instant");
    event.time = numberField(lua, table, "time", 0.0);
    if (event.time < 0.0 || event.time > duration)
        luaL_error(lua, "Interaction event '%s' has an invalid time", event.id.c_str());
    if (event.action == ACTION::TRANSFORM)
    {
        event.x = numberField(lua, table, "x", 0.0);
        event.y = numberField(lua, table, "y", 0.0);
        event.z = numberField(lua, table, "z", 0.0);
        event.rx = numberField(lua, table, "rx", 0.0);
        event.ry = numberField(lua, table, "ry", 0.0);
        event.rz = numberField(lua, table, "rz", 0.0);
        event.sx = numberField(lua, table, "sx", 1.0);
        event.sy = numberField(lua, table, "sy", 1.0);
        event.sz = numberField(lua, table, "sz", 1.0);
        if (event.transition != "instant" && event.transition != "smooth")
            luaL_error(lua, "Interaction event '%s' has an invalid transition", event.id.c_str());
        event.transitionEnd = numberField(lua, table, "transitionEnd", event.time);
        event.transitionStart = numberField(lua, table, "transitionStart",
                                            event.transitionEnd - 0.2);
        if (event.transition == "instant")
        {
            event.transitionStart = event.time;
            event.transitionEnd = event.time;
        }
        if (event.transitionStart < 0.0 || event.transitionEnd < event.transitionStart ||
            event.transitionEnd > duration)
            luaL_error(lua, "Interaction event '%s' has an invalid transition interval", event.id.c_str());
    }
    if (event.action == ACTION::VISIBILITY)
        event.visible = booleanField(lua, table, "visible", true);
}

DEFINITION parseDefinition(lua_State *lua, const int table)
{
    DEFINITION definition;
    const int definitionTable = lua_absindex(lua, table);
    luaL_checktype(lua, definitionTable, LUA_TTABLE);
    lua_getfield(lua, definitionTable, "actors");
    const int actorsTable = lua_absindex(lua, -1);
    luaL_checktype(lua, actorsTable, LUA_TTABLE);
    lua_getfield(lua, definitionTable, "actorOrder");
    const int orderTable = lua_absindex(lua, -1);
    if (lua_istable(lua, orderTable) && lua_rawlen(lua, orderTable) > 0)
    {
        const std::size_t count = lua_rawlen(lua, orderTable);
        for (std::size_t index = 1; index <= count; ++index)
        {
            lua_rawgeti(lua, orderTable, static_cast<lua_Integer>(index));
            const char *idValue = luaL_checkstring(lua, -1);
            const std::string id(idValue);
            lua_pop(lua, 1);
            lua_getfield(lua, actorsTable, id.c_str());
            const int actorTable = lua_absindex(lua, -1);
            luaL_checktype(lua, actorTable, LUA_TTABLE);
            ACTOR_DEFINITION actor;
            actor.id = id;
            actor.name = stringField(lua, actorTable, "name", id.c_str());
            actor.clipKey = stringField(lua, actorTable, "clipKey");
            definition.actorIndex.emplace(actor.id, definition.actors.size());
            definition.actors.push_back(std::move(actor));
            lua_pop(lua, 1);
        }
    }
    else
    {
        lua_pushnil(lua);
        while (lua_next(lua, actorsTable) != 0)
        {
            const char *idValue = luaL_checkstring(lua, -2);
            const std::string id(idValue);
            luaL_checktype(lua, -1, LUA_TTABLE);
            ACTOR_DEFINITION actor;
            actor.id = id;
            actor.name = stringField(lua, -1, "name", id.c_str());
            actor.clipKey = stringField(lua, -1, "clipKey");
            definition.actorIndex.emplace(actor.id, definition.actors.size());
            definition.actors.push_back(std::move(actor));
            lua_pop(lua, 1);
        }
    }
    lua_pop(lua, 2);
    if (definition.actors.empty()) luaL_error(lua, "Interaction definition has no actors");
    definition.anchorActor = stringField(lua, definitionTable, "anchorActor", definition.actors[0].id.c_str());
    definition.clockActor = stringField(lua, definitionTable, "clockActor", definition.actors[0].id.c_str());
    definition.clockKey = stringField(lua, definitionTable, "clockKey");
    definition.continueClipsAfterTimeline = booleanField(
        lua, definitionTable, "continueClipsAfterTimeline", false);
    const double requestedDuration = numberField(
        lua, definitionTable, "duration", -1.0);
    lua_getfield(lua, definitionTable, "events");
    const int eventsTable = lua_absindex(lua, -1);
    if (!lua_isnil(lua, eventsTable)) luaL_checktype(lua, eventsTable, LUA_TTABLE);
    const std::size_t eventCount = lua_istable(lua, eventsTable) ? lua_rawlen(lua, eventsTable) : 0;
    double maximumEventTime = 0.0;
    for (std::size_t index = 1; index <= eventCount; ++index)
    {
        lua_rawgeti(lua, eventsTable, static_cast<lua_Integer>(index));
        luaL_checktype(lua, -1, LUA_TTABLE);
        EVENT event;
        parseEvent(lua, -1, std::numeric_limits<double>::infinity(), event);
        maximumEventTime = std::max(maximumEventTime, event.time);
        definition.events.push_back(std::move(event));
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
    definition.duration = requestedDuration >= 0.0 ? requestedDuration : maximumEventTime;
    if (definition.duration <= 0.0) definition.duration = std::numeric_limits<double>::infinity();
    if (findActor(definition, definition.anchorActor) == std::numeric_limits<std::size_t>::max())
        luaL_error(lua, "Interaction anchor actor does not exist: %s", definition.anchorActor.c_str());
    if (findActor(definition, definition.clockActor) == std::numeric_limits<std::size_t>::max())
        luaL_error(lua, "Interaction clock actor does not exist: %s", definition.clockActor.c_str());
    if (definition.clockKey.empty())
    {
        const std::size_t clockIndex = findActor(definition, definition.clockActor);
        definition.clockKey = definition.actors[clockIndex].clipKey;
    }
    for (EVENT &event : definition.events)
    {
        if (event.time > definition.duration ||
            (event.action == ACTION::TRANSFORM && event.transitionEnd > definition.duration))
            luaL_error(lua, "Interaction event '%s' is outside the interaction timeline", event.id.c_str());
        if (event.action == ACTION::TRANSFORM || event.action == ACTION::PLAY ||
            event.action == ACTION::VISIBILITY)
        {
            if (findActor(definition, event.target) == std::numeric_limits<std::size_t>::max())
                luaL_error(lua, "Interaction event '%s' references an invalid actor", event.id.c_str());
        }
        if (event.action == ACTION::PLAY && event.clipKey.empty())
            luaL_error(lua, "Interaction event '%s' has no animation clip", event.id.c_str());
    }
    return definition;
}

bool getBinding(lua_State *lua, const int bindingsTable, const std::string &name,
                const std::string &fixedName, int &objectReference, mbm::MESH *&mesh)
{
    const char *names[2] = {name.c_str(), fixedName.c_str()};
    for (const char *candidate : names)
    {
        lua_getfield(lua, bindingsTable, candidate);
        if (lua_isnil(lua, -1))
        {
            lua_pop(lua, 1);
            continue;
        }
        const int objectTable = lua_absindex(lua, -1);
        mbm::RENDERIZABLE *renderizable = mbm::getRenderizableFromRawTable(lua, 1, objectTable);
        if (!renderizable || renderizable->getTypeClass() != mbm::TYPE_CLASS_MESH)
            luaL_error(lua, "Interaction actor '%s' must be a mesh", name.c_str());
        mesh = static_cast<mbm::MESH *>(renderizable);
        lua_pushvalue(lua, objectTable);
        objectReference = luaL_ref(lua, LUA_REGISTRYINDEX);
        lua_pop(lua, 1);
        return true;
    }
    return false;
}

INTERACTION_INSTANCE *getInstance(lua_State *lua)
{
    void *raw = mbm::plugin_check_userdata(lua, 1, 1, PLUGIN_IDENTIFIER,
                                           "InteractionAnimationInstance");
    auto **instance = static_cast<INTERACTION_INSTANCE **>(raw);
    if (!instance || !*instance) luaL_error(lua, "Invalid interaction animation instance");
    return *instance;
}

int onInstanceGc(lua_State *lua)
{
    auto **instance = static_cast<INTERACTION_INSTANCE **>(lua_touserdata(lua, 1));
    if (instance && *instance)
    {
        delete *instance;
        *instance = nullptr;
    }
    return 0;
}

int onInstanceUpdate(lua_State *lua)
{
    INTERACTION_INSTANCE *instance = getInstance(lua);
    instance->update(luaL_checknumber(lua, 2));
    lua_pushboolean(lua, !instance->isFinished());
    return 1;
}

int onInstanceRestart(lua_State *lua)
{
    INTERACTION_INSTANCE *instance = getInstance(lua);
    instance->restart();
    lua_pushvalue(lua, 1);
    return 1;
}

int onInstanceStop(lua_State *lua)
{
    INTERACTION_INSTANCE *instance = getInstance(lua);
    instance->stop();
    lua_pushvalue(lua, 1);
    return 1;
}

int onInstanceIsFinished(lua_State *lua)
{
    lua_pushboolean(lua, getInstance(lua)->isFinished());
    return 1;
}

int onStartInteractionLua(lua_State *lua)
{
    const int config = lua_absindex(lua, 1);
    luaL_checktype(lua, config, LUA_TTABLE);
    lua_getfield(lua, config, "definition");
    const int definitionTable = lua_absindex(lua, -1);
    DEFINITION definition = parseDefinition(lua, definitionTable);
    lua_pop(lua, 1);
    lua_getfield(lua, config, "actors");
    const int bindingsTable = lua_absindex(lua, -1);
    luaL_checktype(lua, bindingsTable, LUA_TTABLE);
    auto *instance = new INTERACTION_INSTANCE(lua);
    instance->definition = std::move(definition);
    instance->actors.reserve(instance->definition.actors.size());
    for (const ACTOR_DEFINITION &actorDefinition : instance->definition.actors)
    {
        const std::string fixedName = actorDefinition.id.substr(0, 5) == "actor" &&
            actorDefinition.id.size() > 5
            ? "actor_" + actorDefinition.id.substr(5) : actorDefinition.id;
        int objectReference = LUA_NOREF;
        mbm::MESH *mesh = nullptr;
        if (!getBinding(lua, bindingsTable, actorDefinition.name, fixedName,
                        objectReference, mesh) && actorDefinition.name != actorDefinition.id)
        {
            getBinding(lua, bindingsTable, actorDefinition.id, fixedName,
                       objectReference, mesh);
        }
        if (!mesh)
        {
            const std::string missingActor = actorDefinition.id;
            delete instance;
            lua_pop(lua, 1);
            return luaL_error(lua, "Missing interaction actor binding: %s", missingActor.c_str());
        }
        instance->actors.push_back({actorDefinition, mesh, readTransform(mesh), objectReference, {}});
        instance->actorIndex[actorDefinition.id] = instance->actors.size() - 1;
    }
    lua_pop(lua, 1);
    lua_getfield(lua, config, "callbacks");
    if (!lua_isnil(lua, -1))
    {
        const int callbacks = lua_absindex(lua, -1);
        luaL_checktype(lua, callbacks, LUA_TTABLE);
        int *references[3] = {
            &instance->onStartReference, &instance->onEventReference, &instance->onFinishReference};
        const char *names[3] = {"onStart", "onEvent", "onFinish"};
        for (int index = 0; index < 3; ++index)
        {
            lua_getfield(lua, callbacks, names[index]);
            if (!lua_isnil(lua, -1))
            {
                luaL_checktype(lua, -1, LUA_TFUNCTION);
                *references[index] = luaL_ref(lua, LUA_REGISTRYINDEX);
            }
            else lua_pop(lua, 1);
        }
    }
    lua_pop(lua, 1);
    instance->initializeAxes();
    instance->fired.assign(instance->definition.events.size(), false);
    instance->notified.assign(instance->definition.events.size(), false);

    lua_newtable(lua);
    lua_pushcfunction(lua, onInstanceUpdate); lua_setfield(lua, -2, "update");
    lua_pushcfunction(lua, onInstanceRestart); lua_setfield(lua, -2, "restart");
    lua_pushcfunction(lua, onInstanceStop); lua_setfield(lua, -2, "stop");
    lua_pushcfunction(lua, onInstanceIsFinished); lua_setfield(lua, -2, "isFinished");
    const int instanceTable = lua_absindex(lua, -1);
    auto **userdata = static_cast<INTERACTION_INSTANCE **>(
        lua_newuserdata(lua, sizeof(INTERACTION_INSTANCE *)));
    *userdata = instance;
    mbm::plugin_stamp_userdata(lua, &PLUGIN_IDENTIFIER);
    luaL_getmetatable(lua, INSTANCE_METATABLE);
    lua_pushinteger(lua, PLUGIN_IDENTIFIER);
    lua_rawseti(lua, -2, 1);
    lua_setmetatable(lua, -2);
    lua_rawseti(lua, instanceTable, 1);
    lua_newtable(lua);
    lua_newtable(lua);
    lua_pushliteral(lua, "v");
    lua_setfield(lua, -2, "__mode");
    lua_setmetatable(lua, -2);
    lua_pushlightuserdata(lua, instance);
    lua_pushvalue(lua, instanceTable);
    lua_rawset(lua, -3);
    instance->weakInstanceReference = luaL_ref(lua, LUA_REGISTRYINDEX);
    instance->resetInstance();
    instance->callCallback(instance->onStartReference, nullptr, 0);
    return 1;
}

void ensureInstanceMetatable(lua_State *lua)
{
    if (luaL_newmetatable(lua, INSTANCE_METATABLE))
    {
        lua_pushcfunction(lua, onInstanceGc);
        lua_setfield(lua, -2, "__gc");
    }
    lua_pop(lua, 1);
}

const luaL_Reg MODULE_FUNCTIONS[] = {
    {"start", onStartInteractionLua},
    {nullptr, nullptr},
};

}

extern "C" INTERACTION_ANIMATION_RUNTIME_API int luaopen_interaction_animation_runtime(lua_State *lua)
{
    ensureInstanceMetatable(lua);
    luaL_newlib(lua, MODULE_FUNCTIONS);
    return 1;
}

extern "C" INTERACTION_ANIMATION_RUNTIME_API int luaopen_libinteraction_animation_runtime(lua_State *lua)
{
    return luaopen_interaction_animation_runtime(lua);
}
