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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#include "attachments-lua.h"

#include <plugin-helper/plugin-helper.h>
#include <core_mbm/plugin-callback.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/util-interface.h>
#include <render/mesh.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

constexpr const char *PLUGIN_METATABLE = "_mbmAttachmentsPlugin";
constexpr const char *ATTACHMENT_METATABLE = "_mbmAttachmentsAttachment";
constexpr uint8_t INHERIT_POSITION = 1u;
constexpr uint8_t INHERIT_ROTATION = 2u;
constexpr uint8_t INHERIT_SCALE = 4u;
constexpr uint8_t SOURCE_ROOT = 0u;
constexpr uint8_t SOURCE_BONE = 1u;

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

struct POSE
{
    VEC3D position;
    VEC3D angle;
    VEC3D scale{1.0, 1.0, 1.0};
};

bool isFinite(const VEC3D &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct ATTACHMENT;
class ATTACHMENTS_PLUGIN;

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

MATRIX3 transpose(const MATRIX3 &matrix)
{
    MATRIX3 result;
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
            result.value[row][column] = matrix.value[column][row];
    }
    return result;
}

VEC3D transformVector(const MATRIX3 &matrix, const VEC3D &vector)
{
    return {
        matrix.value[0][0] * vector.x + matrix.value[1][0] * vector.y +
            matrix.value[2][0] * vector.z,
        matrix.value[0][1] * vector.x + matrix.value[1][1] * vector.y +
            matrix.value[2][1] * vector.z,
        matrix.value[0][2] * vector.x + matrix.value[1][2] * vector.y +
            matrix.value[2][2] * vector.z,
    };
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

POSE readPose(const mbm::RENDERIZABLE *renderizable)
{
    const mbm::VEC3 &position = renderizable->getPosition();
    const mbm::VEC3 &angle = renderizable->getAngle();
    const mbm::VEC3 &scale = renderizable->getScale();
    return {{position.x, position.y, position.z},
            {angle.x, angle.y, angle.z},
            {scale.x, scale.y, scale.z}};
}

void writePosition(mbm::RENDERIZABLE *renderizable, const VEC3D &position)
{
    renderizable->setPosition({static_cast<float>(position.x),
                               static_cast<float>(position.y),
                               static_cast<float>(position.z)});
}

void writeAngle(mbm::RENDERIZABLE *renderizable, const VEC3D &angle)
{
    renderizable->setAngle({static_cast<float>(angle.x),
                            static_cast<float>(angle.y),
                            static_cast<float>(angle.z)});
}

void writeScale(mbm::RENDERIZABLE *renderizable, const VEC3D &scale)
{
    renderizable->setScale({static_cast<float>(scale.x),
                            static_cast<float>(scale.y),
                            static_cast<float>(scale.z)});
}

bool isValidLuaRenderizable(mbm::RENDERIZABLE *object)
{
    // The attachment owns the Lua table reference. ref_MeAsTable is optional
    // userdata state used by callback paths, not a general lifetime marker.
    return object != nullptr;
}

void pushVec3(lua_State *lua, const VEC3D &value)
{
    lua_createtable(lua, 0, 3);
    lua_pushnumber(lua, value.x); lua_setfield(lua, -2, "x");
    lua_pushnumber(lua, value.y); lua_setfield(lua, -2, "y");
    lua_pushnumber(lua, value.z); lua_setfield(lua, -2, "z");
}

double numberField(lua_State *lua, const int table, const char *field, const double fallback)
{
    lua_getfield(lua, table, field);
    const double value = lua_isnumber(lua, -1) ? lua_tonumber(lua, -1) : fallback;
    lua_pop(lua, 1);
    return value;
}

VEC3D vec3Field(lua_State *lua, const int table, const char *field, const VEC3D &fallback)
{
    lua_getfield(lua, table, field);
    const int valueTable = lua_absindex(lua, -1);
    VEC3D value = fallback;
    if (lua_istable(lua, valueTable))
    {
        value.x = numberField(lua, valueTable, "x", value.x);
        value.y = numberField(lua, valueTable, "y", value.y);
        value.z = numberField(lua, valueTable, "z", value.z);
    }
    lua_pop(lua, 1);
    return value;
}

bool booleanField(lua_State *lua, const int table, const char *field, const bool fallback)
{
    lua_getfield(lua, table, field);
    const bool value = lua_isboolean(lua, -1) ? lua_toboolean(lua, -1) != 0 : fallback;
    lua_pop(lua, 1);
    return value;
}

uint8_t parseInheritance(lua_State *lua, const int spec)
{
    uint8_t result = INHERIT_POSITION | INHERIT_ROTATION | INHERIT_SCALE;
    lua_getfield(lua, spec, "inherit");
    const int inherit = lua_absindex(lua, -1);
    if (lua_istable(lua, inherit))
    {
        if (!booleanField(lua, inherit, "position", true)) result &= ~INHERIT_POSITION;
        if (!booleanField(lua, inherit, "rotation", true)) result &= ~INHERIT_ROTATION;
        if (!booleanField(lua, inherit, "scale", true)) result &= ~INHERIT_SCALE;
    }
    lua_pop(lua, 1);
    return result;
}

POSE parseOffset(lua_State *lua, const int spec)
{
    POSE result;
    lua_getfield(lua, spec, "offset");
    const int offset = lua_absindex(lua, -1);
    if (lua_istable(lua, offset))
    {
        result.position = vec3Field(lua, offset, "position", result.position);
        result.angle = vec3Field(lua, offset, "rotation", result.angle);
        result.scale = vec3Field(lua, offset, "scale", result.scale);
    }
    lua_pop(lua, 1);
    return result;
}

struct ATTACHMENT
{
    ATTACHMENTS_PLUGIN *manager = nullptr;
    lua_State *lua = nullptr;
    int sourceReference = LUA_NOREF;
    int targetReference = LUA_NOREF;
    std::string id;
    std::string sourceBone;
    uint8_t sourceType = SOURCE_ROOT;
    POSE offset;
    POSE poseBeforeAttach;
    uint8_t inherit = INHERIT_POSITION | INHERIT_ROTATION | INHERIT_SCALE;
    bool active = true;
    bool invalid = false;

    void releaseReferences() noexcept;
};

bool readBonePose(mbm::RENDERIZABLE *source, const std::string &boneName, POSE &pose)
{
    if (!source || source->getTypeClass() != mbm::TYPE_CLASS_MESH || boneName.empty()) return false;
    auto *mesh = dynamic_cast<mbm::MESH *>(source);
    if (!mesh) return false;
    uint64_t boneId = 0;
    mbm::MATRIX matrix;
    mbm::VEC3 position;
    mbm::VEC3 angle;
    mbm::VEC3 scale;
    float rotation[4] = {};
    if (!mesh->getSkeletalBoneTransform(boneName.c_str(), true, &boneId, &matrix, &position,
                                        rotation, &angle, &scale))
        return false;
    pose.position = {position.x, position.y, position.z};
    pose.angle = {angle.x, angle.y, angle.z};
    pose.scale = {scale.x, scale.y, scale.z};
    return isFinite(pose.position) && isFinite(pose.angle) && isFinite(pose.scale);
}

bool readSourcePose(const ATTACHMENT *attachment, mbm::RENDERIZABLE *source, POSE &pose)
{
    if (!attachment || !source) return false;
    if (attachment->sourceType == SOURCE_ROOT)
    {
        pose = readPose(source);
        return true;
    }
    if (attachment->sourceType == SOURCE_BONE)
        return readBonePose(source, attachment->sourceBone, pose);
    return false;
}

POSE composeEffectivePose(const ATTACHMENT *attachment, const POSE &sourcePose,
                          const POSE &currentTargetPose)
{
    POSE result = currentTargetPose;
    if (!attachment) return result;
    const MATRIX3 sourceRotation = eulerRotation(sourcePose.angle);
    if ((attachment->inherit & INHERIT_POSITION) != 0u)
    {
        VEC3D local = attachment->offset.position;
        if ((attachment->inherit & INHERIT_SCALE) != 0u)
        {
            local.x *= sourcePose.scale.x;
            local.y *= sourcePose.scale.y;
            local.z *= sourcePose.scale.z;
        }
        const VEC3D rotated = transformVector(sourceRotation, local);
        result.position = {sourcePose.position.x + rotated.x,
                           sourcePose.position.y + rotated.y,
                           sourcePose.position.z + rotated.z};
    }
    if ((attachment->inherit & INHERIT_ROTATION) != 0u)
    {
        result.angle = rotationToEuler(multiply(sourceRotation,
                                                eulerRotation(attachment->offset.angle)));
    }
    if ((attachment->inherit & INHERIT_SCALE) != 0u)
    {
        result.scale = {sourcePose.scale.x * attachment->offset.scale.x,
                        sourcePose.scale.y * attachment->offset.scale.y,
                        sourcePose.scale.z * attachment->offset.scale.z};
    }
    return result;
}

struct RESOLVED_ATTACHMENT
{
    ATTACHMENT *attachment = nullptr;
    mbm::RENDERIZABLE *source = nullptr;
    mbm::RENDERIZABLE *target = nullptr;
};

class ATTACHMENTS_PLUGIN final : public PLUGIN
{
  public:
    explicit ATTACHMENTS_PLUGIN(lua_State *state) noexcept : lua(state) {}

    ~ATTACHMENTS_PLUGIN() override
    {
        onDestroy();
    }

    void onSubscribe(int, int, void *, void *) override { active = true; }
    void onResizeWindow(int, int) override {}
    void onTouchDown(int, float, float) override {}
    void onTouchUp(int, float, float) override {}
    void onTouchMove(int, float, float) override {}
    void onTouchZoom(float) override {}
    void onKeyDown(int) override {}
    void onKeyUp(int) override {}
    void onDoubleClick(float, float, int) override {}
    void onKeyDownJoystick(int, int) override {}
    void onKeyUpJoystick(int, int) override {}
    void onMoveJoystick(int, float, float, float, float) override {}
    void onInfoDeviceJoystick(int, int, const char *, const char *) override {}
    void onPrepare() override {}
    void onRender() override {}

    void onLoop(float) override;
    void onDestroy() override;

    std::shared_ptr<ATTACHMENT> createAttachment(const std::string &id,
                                                  const uint8_t sourceType,
                                                  const std::string &sourceBone,
                                                  const POSE &offset,
                                                  const POSE &before,
                                                  uint8_t inherit,
                                                  int sourceReference,
                                                  int targetReference);
    bool canReconfigure(ATTACHMENT *attachment, uint8_t newInherit, std::string &error) const;
    bool hasId(const std::string &id) const;
    bool resolve(ATTACHMENT *attachment, mbm::RENDERIZABLE **source,
                 mbm::RENDERIZABLE **target) const;
    void deactivate(ATTACHMENT *attachment, const bool invalid);
    bool detach(ATTACHMENT *attachment, const bool preserveWorldPose);
    bool getEffectivePose(ATTACHMENT *attachment, POSE &pose) const;
    void clearReferences(ATTACHMENT *attachment) noexcept;
    bool isActive() const noexcept { return active; }
    lua_State *getLua() const noexcept { return lua; }
    std::string nextAttachmentId() { return "attachment_" + std::to_string(nextId++); }
    bool wouldCycle(mbm::RENDERIZABLE *source, mbm::RENDERIZABLE *target,
                    const ATTACHMENT *ignore) const;
    bool hasConflict(mbm::RENDERIZABLE *target, uint8_t inherit,
                     const ATTACHMENT *ignore) const;

  private:
    lua_State *lua = nullptr;
    std::vector<std::shared_ptr<ATTACHMENT>> attachments;
    bool active = false;
    bool destroying = false;
    uint64_t nextId = 1;

    bool resolveAll(std::vector<RESOLVED_ATTACHMENT> &resolved);
    void releaseAllReferences() noexcept;
};

void ATTACHMENT::releaseReferences() noexcept
{
    if (!lua) return;
    if (sourceReference != LUA_NOREF)
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        sourceReference = LUA_NOREF;
    }
    if (targetReference != LUA_NOREF)
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
        targetReference = LUA_NOREF;
    }
    lua = nullptr;
}

bool ATTACHMENTS_PLUGIN::resolve(ATTACHMENT *attachment, mbm::RENDERIZABLE **source,
                                 mbm::RENDERIZABLE **target) const
{
    if (!attachment || !lua || attachment->sourceReference == LUA_NOREF ||
        attachment->targetReference == LUA_NOREF)
        return false;
    lua_rawgeti(lua, LUA_REGISTRYINDEX, attachment->sourceReference);
    const int sourceTable = lua_absindex(lua, -1);
    mbm::RENDERIZABLE *sourceObject = lua_istable(lua, sourceTable)
        ? mbm::getRenderizableNoThrowFromRawTable(lua, 1, sourceTable) : nullptr;
    lua_pop(lua, 1);
    lua_rawgeti(lua, LUA_REGISTRYINDEX, attachment->targetReference);
    const int targetTable = lua_absindex(lua, -1);
    mbm::RENDERIZABLE *targetObject = lua_istable(lua, targetTable)
        ? mbm::getRenderizableNoThrowFromRawTable(lua, 1, targetTable) : nullptr;
    lua_pop(lua, 1);
    if (!isValidLuaRenderizable(sourceObject) || !isValidLuaRenderizable(targetObject))
        return false;
    *source = sourceObject;
    *target = targetObject;
    return true;
}

bool ATTACHMENTS_PLUGIN::getEffectivePose(ATTACHMENT *attachment, POSE &pose) const
{
    mbm::RENDERIZABLE *source = nullptr;
    mbm::RENDERIZABLE *target = nullptr;
    if (!resolve(attachment, &source, &target) || source->getIdScene() != target->getIdScene())
        return false;
    POSE sourcePose;
    if (!readSourcePose(attachment, source, sourcePose)) return false;
    pose = composeEffectivePose(attachment, sourcePose, readPose(target));
    return true;
}

void ATTACHMENTS_PLUGIN::clearReferences(ATTACHMENT *attachment) noexcept
{
    if (!attachment) return;
    attachment->releaseReferences();
}

void ATTACHMENTS_PLUGIN::deactivate(ATTACHMENT *attachment, const bool invalid)
{
    if (!attachment) return;
    attachment->active = false;
    attachment->invalid = invalid;
    clearReferences(attachment);
}

bool ATTACHMENTS_PLUGIN::hasConflict(mbm::RENDERIZABLE *target, const uint8_t inherit,
                                     const ATTACHMENT *ignore) const
{
    for (const std::shared_ptr<ATTACHMENT> &candidate : attachments)
    {
        if (!candidate || candidate.get() == ignore || !candidate->active) continue;
        mbm::RENDERIZABLE *sourceObject = nullptr;
        mbm::RENDERIZABLE *targetObject = nullptr;
        if (!resolve(candidate.get(), &sourceObject, &targetObject)) continue;
        if (targetObject == target && (candidate->inherit & inherit) != 0u) return true;
    }
    return false;
}

bool ATTACHMENTS_PLUGIN::hasId(const std::string &id) const
{
    for (const std::shared_ptr<ATTACHMENT> &attachment : attachments)
    {
        if (attachment && attachment->id == id && attachment->active) return true;
    }
    return false;
}

bool ATTACHMENTS_PLUGIN::wouldCycle(mbm::RENDERIZABLE *source,
                                    mbm::RENDERIZABLE *target,
                                    const ATTACHMENT *ignore) const
{
    std::unordered_map<mbm::RENDERIZABLE *, std::vector<mbm::RENDERIZABLE *>> edges;
    for (const std::shared_ptr<ATTACHMENT> &candidate : attachments)
    {
        if (!candidate || candidate.get() == ignore || !candidate->active) continue;
        mbm::RENDERIZABLE *candidateSource = nullptr;
        mbm::RENDERIZABLE *candidateTarget = nullptr;
        if (resolve(candidate.get(), &candidateSource, &candidateTarget))
            edges[candidateSource].push_back(candidateTarget);
    }
    std::vector<mbm::RENDERIZABLE *> pending{target};
    std::vector<mbm::RENDERIZABLE *> visited;
    while (!pending.empty())
    {
        mbm::RENDERIZABLE *current = pending.back();
        pending.pop_back();
        if (current == source) return true;
        if (std::find(visited.begin(), visited.end(), current) != visited.end()) continue;
        visited.push_back(current);
        const auto found = edges.find(current);
        if (found != edges.end())
            pending.insert(pending.end(), found->second.begin(), found->second.end());
    }
    return false;
}

std::shared_ptr<ATTACHMENT> ATTACHMENTS_PLUGIN::createAttachment(
    const std::string &id, const uint8_t sourceType, const std::string &sourceBone,
    const POSE &offset, const POSE &before, const uint8_t inherit, const int sourceReference,
    const int targetReference)
{
    std::shared_ptr<ATTACHMENT> attachment = std::make_shared<ATTACHMENT>();
    attachment->manager = this;
    attachment->lua = lua;
    attachment->sourceReference = sourceReference;
    attachment->targetReference = targetReference;
    attachment->id = id;
    attachment->sourceType = sourceType;
    attachment->sourceBone = sourceBone;
    attachment->offset = offset;
    attachment->poseBeforeAttach = before;
    attachment->inherit = inherit;
    attachments.push_back(attachment);
    return attachment;
}

bool ATTACHMENTS_PLUGIN::canReconfigure(ATTACHMENT *attachment, const uint8_t newInherit,
                                        std::string &error) const
{
    if (!attachment || !attachment->active)
    {
        error = "attachment is not active";
        return false;
    }
    mbm::RENDERIZABLE *sourceObject = nullptr;
    mbm::RENDERIZABLE *targetObject = nullptr;
    if (!resolve(attachment, &sourceObject, &targetObject))
    {
        error = "source or target is no longer valid";
        return false;
    }
    if (hasConflict(targetObject, newInherit, attachment))
    {
        error = "target component is already owned by another attachment";
        return false;
    }
    if (wouldCycle(sourceObject, targetObject, attachment))
    {
        error = "attachment would create a cycle";
        return false;
    }
    return true;
}

bool ATTACHMENTS_PLUGIN::detach(ATTACHMENT *attachment, const bool preserveWorldPose)
{
    if (!attachment || !attachment->active) return false;
    mbm::RENDERIZABLE *sourceObject = nullptr;
    mbm::RENDERIZABLE *targetObject = nullptr;
    if (!resolve(attachment, &sourceObject, &targetObject))
    {
        deactivate(attachment, true);
        return false;
    }
    if (!preserveWorldPose)
    {
        writePosition(targetObject, attachment->poseBeforeAttach.position);
        writeAngle(targetObject, attachment->poseBeforeAttach.angle);
        writeScale(targetObject, attachment->poseBeforeAttach.scale);
    }
    attachment->active = false;
    clearReferences(attachment);
    return true;
}

bool ATTACHMENTS_PLUGIN::resolveAll(std::vector<RESOLVED_ATTACHMENT> &resolved)
{
    for (const std::shared_ptr<ATTACHMENT> &attachment : attachments)
    {
        if (!attachment || !attachment->active) continue;
        mbm::RENDERIZABLE *sourceObject = nullptr;
        mbm::RENDERIZABLE *targetObject = nullptr;
        if (!resolve(attachment.get(), &sourceObject, &targetObject))
        {
            deactivate(attachment.get(), true);
            continue;
        }
        if (sourceObject->getIdScene() != targetObject->getIdScene())
        {
            deactivate(attachment.get(), true);
            continue;
        }
        resolved.push_back({attachment.get(), sourceObject, targetObject});
    }
    return true;
}

void ATTACHMENTS_PLUGIN::onLoop(float)
{
    if (!active || destroying) return;
    std::vector<RESOLVED_ATTACHMENT> resolved;
    resolveAll(resolved);
    std::vector<std::size_t> order;
    std::vector<unsigned int> indegree(resolved.size(), 0u);
    for (std::size_t i = 0; i < resolved.size(); ++i)
    {
        for (std::size_t j = 0; j < resolved.size(); ++j)
        {
            if (i != j && resolved[i].target == resolved[j].source)
                ++indegree[j];
        }
    }
    while (order.size() < resolved.size())
    {
        std::size_t next = resolved.size();
        for (std::size_t i = 0; i < resolved.size(); ++i)
        {
            if (indegree[i] == 0u &&
                std::find(order.begin(), order.end(), i) == order.end())
            {
                next = i;
                break;
            }
        }
        if (next == resolved.size())
        {
            ERROR_LOG("attachments: cycle detected during update; disabling unresolved attachments");
            for (std::size_t i = 0; i < resolved.size(); ++i)
            {
                if (std::find(order.begin(), order.end(), i) == order.end())
                    deactivate(resolved[i].attachment, true);
            }
            break;
        }
        order.push_back(next);
        for (std::size_t i = 0; i < resolved.size(); ++i)
        {
            if (indegree[i] != 0u && resolved[next].target == resolved[i].source)
                --indegree[i];
        }
    }
    for (const std::size_t index : order)
    {
        const RESOLVED_ATTACHMENT &resolvedAttachment = resolved[index];
        ATTACHMENT *attachment = resolvedAttachment.attachment;
        POSE sourcePose;
        if (!readSourcePose(attachment, resolvedAttachment.source, sourcePose))
        {
            deactivate(attachment, true);
            continue;
        }
        const POSE effectivePose = composeEffectivePose(attachment, sourcePose,
                                                        readPose(resolvedAttachment.target));
        if ((attachment->inherit & INHERIT_POSITION) != 0u) writePosition(resolvedAttachment.target,
                                                                            effectivePose.position);
        if ((attachment->inherit & INHERIT_ROTATION) != 0u) writeAngle(resolvedAttachment.target,
                                                                        effectivePose.angle);
        if ((attachment->inherit & INHERIT_SCALE) != 0u) writeScale(resolvedAttachment.target,
                                                                    effectivePose.scale);
    }
}

void ATTACHMENTS_PLUGIN::releaseAllReferences() noexcept
{
    for (const std::shared_ptr<ATTACHMENT> &attachment : attachments)
    {
        if (!attachment) continue;
        attachment->active = false;
        attachment->manager = nullptr;
        attachment->releaseReferences();
    }
}

void ATTACHMENTS_PLUGIN::onDestroy()
{
    if (destroying) return;
    destroying = true;
    active = false;
    releaseAllReferences();
    destroying = false;
}

int fail(lua_State *lua, const char *message)
{
    lua_pushnil(lua);
    lua_pushstring(lua, message);
    return 2;
}

bool getRenderizableField(lua_State *lua, const int spec, const char *field,
                          mbm::RENDERIZABLE **renderizable, int *reference,
                          std::string &error)
{
    lua_getfield(lua, spec, field);
    const int objectTable = lua_absindex(lua, -1);
    if (!lua_istable(lua, objectTable))
    {
        lua_pop(lua, 1);
        error = std::string(field) + " must be a renderizable object";
        return false;
    }
    *renderizable = mbm::getRenderizableNoThrowFromRawTable(lua, 1, objectTable);
    if (!isValidLuaRenderizable(*renderizable))
    {
        lua_pop(lua, 1);
        error = std::string(field) + " is invalid or destroyed";
        return false;
    }
    // Keep the renderizable table alive for the attachment lifetime.
    *reference = luaL_ref(lua, LUA_REGISTRYINDEX);
    return true;
}

ATTACHMENT *getAttachment(lua_State *lua, const int index)
{
    auto *holder = static_cast<std::shared_ptr<ATTACHMENT> *>(
        mbm::plugin_check_userdata(lua, 1, index, PLUGIN_IDENTIFIER, "attachment"));
    return holder && *holder ? holder->get() : nullptr;
}

int onAttachmentGc(lua_State *lua)
{
    if (lua_istable(lua, 1))
    {
        lua_rawgeti(lua, 1, 1);
        auto *holder = static_cast<std::shared_ptr<ATTACHMENT> *>(lua_touserdata(lua, -1));
        if (holder) holder->~shared_ptr<ATTACHMENT>();
        lua_pop(lua, 1);
    }
    return 0;
}

int onAttachmentDetach(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    if (!attachment || !attachment->manager)
    {
        lua_pushboolean(lua, 0);
        return 1;
    }
    bool preserve = true;
    if (lua_gettop(lua) >= 2 && lua_istable(lua, 2))
        preserve = booleanField(lua, 2, "preserveWorldPose", true);
    lua_pushboolean(lua, attachment->manager->detach(attachment, preserve) ? 1 : 0);
    return 1;
}

int onAttachmentIsActive(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    lua_pushboolean(lua, attachment && attachment->active && attachment->manager ? 1 : 0);
    return 1;
}

int onAttachmentGetId(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    if (!attachment)
    {
        lua_pushnil(lua);
        return 1;
    }
    lua_pushlstring(lua, attachment->id.c_str(), attachment->id.size());
    return 1;
}

void pushPoseData(lua_State *lua, const POSE &pose)
{
    lua_createtable(lua, 0, 4);
    pushVec3(lua, pose.position); lua_setfield(lua, -2, "position");
    pushVec3(lua, pose.angle); lua_setfield(lua, -2, "rotation");
    pushVec3(lua, pose.scale); lua_setfield(lua, -2, "scale");
}

void pushAttachmentData(lua_State *lua, const ATTACHMENT *attachment)
{
    pushPoseData(lua, attachment->offset);
}

int onAttachmentGetOffset(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    if (!attachment)
    {
        lua_pushnil(lua);
        return 1;
    }
    pushAttachmentData(lua, attachment);
    return 1;
}

int onAttachmentSetOffset(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    if (!attachment || !lua_istable(lua, 2)) return fail(lua, "invalid attachment or offset");
    const VEC3D position = vec3Field(lua, 2, "position", attachment->offset.position);
    const VEC3D angle = vec3Field(lua, 2, "rotation", attachment->offset.angle);
    const VEC3D scale = vec3Field(lua, 2, "scale", attachment->offset.scale);
    if (!isFinite(position) || !isFinite(angle) || !isFinite(scale))
        return fail(lua, "offset values must be finite");
    attachment->offset.position = position;
    attachment->offset.angle = angle;
    attachment->offset.scale = scale;
    lua_pushboolean(lua, 1);
    return 1;
}

int onAttachmentSetInherit(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    if (!attachment || !attachment->manager || !lua_istable(lua, 2))
        return fail(lua, "invalid attachment or inherit table");
    uint8_t inherit = attachment->inherit;
    if (booleanField(lua, 2, "position", (inherit & INHERIT_POSITION) != 0u))
        inherit |= INHERIT_POSITION;
    else inherit &= ~INHERIT_POSITION;
    if (booleanField(lua, 2, "rotation", (inherit & INHERIT_ROTATION) != 0u))
        inherit |= INHERIT_ROTATION;
    else inherit &= ~INHERIT_ROTATION;
    if (booleanField(lua, 2, "scale", (inherit & INHERIT_SCALE) != 0u))
        inherit |= INHERIT_SCALE;
    else inherit &= ~INHERIT_SCALE;
    std::string error;
    if (!attachment->manager->canReconfigure(attachment, inherit, error))
        return fail(lua, error.c_str());
    attachment->inherit = inherit;
    lua_pushboolean(lua, 1);
    return 1;
}

int onAttachmentCaptureOffset(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    if (!attachment || !attachment->manager)
        return fail(lua, "attachment is no longer valid");
    mbm::RENDERIZABLE *source = nullptr;
    mbm::RENDERIZABLE *target = nullptr;
    if (!attachment->manager->resolve(attachment, &source, &target))
        return fail(lua, "source or target is no longer valid");
    POSE sourcePose;
    if (!readSourcePose(attachment, source, sourcePose))
        return fail(lua, "source bone is invalid or has no evaluated pose");
    const POSE targetPose = readPose(target);
    const bool scaleIsInherited = (attachment->inherit & INHERIT_SCALE) != 0u;
    if (scaleIsInherited && (std::abs(sourcePose.scale.x) < 0.000001 ||
        std::abs(sourcePose.scale.y) < 0.000001 ||
        std::abs(sourcePose.scale.z) < 0.000001))
        return fail(lua, "cannot capture offset from a zero source scale");
    const MATRIX3 sourceRotation = eulerRotation(sourcePose.angle);
    const VEC3D worldDelta{targetPose.position.x - sourcePose.position.x,
                           targetPose.position.y - sourcePose.position.y,
                           targetPose.position.z - sourcePose.position.z};
    const VEC3D localDelta = transformVector(transpose(sourceRotation), worldDelta);
    attachment->offset.position = scaleIsInherited
        ? VEC3D{localDelta.x / sourcePose.scale.x,
                localDelta.y / sourcePose.scale.y,
                localDelta.z / sourcePose.scale.z}
        : localDelta;
    attachment->offset.angle = rotationToEuler(multiply(transpose(sourceRotation),
                                                        eulerRotation(targetPose.angle)));
    attachment->offset.scale = scaleIsInherited
        ? VEC3D{targetPose.scale.x / sourcePose.scale.x,
                targetPose.scale.y / sourcePose.scale.y,
                targetPose.scale.z / sourcePose.scale.z}
        : targetPose.scale;
    lua_pushboolean(lua, 1);
    return 1;
}

int onAttachmentGetEffectivePose(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    POSE pose;
    if (!attachment || !attachment->manager || !attachment->manager->getEffectivePose(attachment, pose))
        return fail(lua, "source or target is no longer valid");
    pushPoseData(lua, pose);
    return 1;
}

int onAttachmentGetState(lua_State *lua)
{
    ATTACHMENT *attachment = getAttachment(lua, 1);
    if (!attachment)
    {
        lua_pushnil(lua);
        return 1;
    }
    lua_createtable(lua, 0, 8);
    lua_pushboolean(lua, attachment->active && attachment->manager ? 1 : 0);
    lua_setfield(lua, -2, "active");
    lua_pushboolean(lua, attachment->invalid ? 1 : 0);
    lua_setfield(lua, -2, "invalid");
    lua_pushstring(lua, attachment->invalid ? "invalid" : (attachment->active ? "active" : "detached"));
    lua_setfield(lua, -2, "status");
    lua_pushboolean(lua, (attachment->inherit & INHERIT_POSITION) != 0u);
    lua_setfield(lua, -2, "inheritPosition");
    lua_pushboolean(lua, (attachment->inherit & INHERIT_ROTATION) != 0u);
    lua_setfield(lua, -2, "inheritRotation");
    lua_pushboolean(lua, (attachment->inherit & INHERIT_SCALE) != 0u);
    lua_setfield(lua, -2, "inheritScale");
    lua_pushstring(lua, attachment->sourceType == SOURCE_BONE ? "bone" : "root");
    lua_setfield(lua, -2, "sourceType");
    if (attachment->sourceType == SOURCE_BONE)
    {
        lua_pushlstring(lua, attachment->sourceBone.c_str(), attachment->sourceBone.size());
        lua_setfield(lua, -2, "sourceBone");
    }
    pushAttachmentData(lua, attachment);
    lua_setfield(lua, -2, "offset");
    POSE effectivePose;
    if (attachment->manager && attachment->manager->getEffectivePose(attachment, effectivePose))
    {
        pushPoseData(lua, effectivePose);
        lua_setfield(lua, -2, "effectivePose");
    }
    return 1;
}

int onAttach(lua_State *lua)
{
    auto *plugin = static_cast<ATTACHMENTS_PLUGIN *>(lua_touserdata(lua, lua_upvalueindex(1)));
    if (!plugin || !plugin->isActive() || !lua_istable(lua, 1))
        return fail(lua, "attachments plugin is not active or specification is not a table");
    std::string error;
    mbm::RENDERIZABLE *source = nullptr;
    mbm::RENDERIZABLE *target = nullptr;
    int sourceReference = LUA_NOREF;
    int targetReference = LUA_NOREF;
    if (!getRenderizableField(lua, 1, "source", &source, &sourceReference, error))
        return fail(lua, error.c_str());
    if (!getRenderizableField(lua, 1, "target", &target, &targetReference, error))
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        return fail(lua, error.c_str());
    }
    if (source == target)
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
        return fail(lua, "source and target must be different");
    }
    if (source->getIdScene() != target->getIdScene())
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
        return fail(lua, "source and target must belong to the same scene");
    }
    uint8_t sourceType = SOURCE_ROOT;
    std::string sourceBone;
    lua_getfield(lua, 1, "sourceType");
    if (!lua_isnil(lua, -1))
    {
        if (!lua_isstring(lua, -1))
        {
            lua_pop(lua, 1);
            luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
            luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
            return fail(lua, "sourceType must be 'root' or 'bone'");
        }
        const std::string sourceTypeName = lua_tostring(lua, -1);
        if (sourceTypeName == "bone") sourceType = SOURCE_BONE;
        else if (sourceTypeName != "root")
        {
            lua_pop(lua, 1);
            luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
            luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
            return fail(lua, "sourceType must be 'root' or 'bone'; sockets are not supported yet");
        }
    }
    lua_pop(lua, 1);
    if (sourceType == SOURCE_BONE)
    {
        lua_getfield(lua, 1, "sourceBone");
        if (!lua_isstring(lua, -1) || lua_tostring(lua, -1)[0] == '\0')
        {
            lua_pop(lua, 1);
            luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
            luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
            return fail(lua, "sourceBone must be a non-empty bone name for sourceType='bone'");
        }
        sourceBone = lua_tostring(lua, -1);
        lua_pop(lua, 1);
        if (source->getTypeClass() != mbm::TYPE_CLASS_MESH)
        {
            luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
            luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
            return fail(lua, "sourceType='bone' requires a mesh source");
        }
        POSE sourcePose;
        if (!readBonePose(source, sourceBone, sourcePose))
        {
            luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
            luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
            return fail(lua, "source bone is invalid or has no evaluated pose");
        }
    }
    const uint8_t inherit = parseInheritance(lua, 1);
    const POSE offset = parseOffset(lua, 1);
    if (!isFinite(offset.position) || !isFinite(offset.angle) || !isFinite(offset.scale))
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
        return fail(lua, "offset values must be finite");
    }
    const POSE before = readPose(target);
    if (plugin->hasConflict(target, inherit, nullptr))
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
        return fail(lua, "target component is already owned by another attachment");
    }
    if (plugin->wouldCycle(source, target, nullptr))
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
        return fail(lua, "attachment would create a cycle");
    }
    std::string id = plugin->nextAttachmentId();
    lua_getfield(lua, 1, "id");
    if (lua_isstring(lua, -1)) id = lua_tostring(lua, -1);
    lua_pop(lua, 1);
    if (id.empty())
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
        return fail(lua, "attachment id must not be empty");
    }
    if (plugin->hasId(id))
    {
        luaL_unref(lua, LUA_REGISTRYINDEX, sourceReference);
        luaL_unref(lua, LUA_REGISTRYINDEX, targetReference);
        return fail(lua, "attachment id is already active");
    }
    const std::shared_ptr<ATTACHMENT> attachment = plugin->createAttachment(
        id, sourceType, sourceBone, offset, before, inherit, sourceReference, targetReference);
    lua_createtable(lua, 0, 8);
    lua_pushcfunction(lua, onAttachmentDetach); lua_setfield(lua, -2, "detach");
    lua_pushcfunction(lua, onAttachmentIsActive); lua_setfield(lua, -2, "isActive");
    lua_pushcfunction(lua, onAttachmentGetId); lua_setfield(lua, -2, "getId");
    lua_pushcfunction(lua, onAttachmentGetOffset); lua_setfield(lua, -2, "getOffset");
    lua_pushcfunction(lua, onAttachmentSetOffset); lua_setfield(lua, -2, "setOffset");
    lua_pushcfunction(lua, onAttachmentSetInherit); lua_setfield(lua, -2, "setInherit");
    lua_pushcfunction(lua, onAttachmentCaptureOffset); lua_setfield(lua, -2, "captureOffset");
    lua_pushcfunction(lua, onAttachmentGetEffectivePose); lua_setfield(lua, -2, "getEffectivePose");
    lua_pushcfunction(lua, onAttachmentGetState); lua_setfield(lua, -2, "getState");
    lua_newuserdata(lua, sizeof(std::shared_ptr<ATTACHMENT>));
    auto *holder = static_cast<std::shared_ptr<ATTACHMENT> *>(lua_touserdata(lua, -1));
    new (holder) std::shared_ptr<ATTACHMENT>(attachment);
    mbm::plugin_stamp_userdata(lua, &PLUGIN_IDENTIFIER);
    lua_rawseti(lua, -2, 1);
    luaL_newmetatable(lua, ATTACHMENT_METATABLE);
    if (luaL_getmetafield(lua, -1, "__gc") == LUA_TNIL)
    {
        lua_pop(lua, 1);
        lua_pushcfunction(lua, onAttachmentGc);
        lua_setfield(lua, -2, "__gc");
    }
    else lua_pop(lua, 1);
    lua_setmetatable(lua, -2);
    return 1;
}

int onPluginGc(lua_State *lua)
{
    lua_rawgeti(lua, 1, 1);
    auto **plugin = static_cast<ATTACHMENTS_PLUGIN **>(lua_touserdata(lua, -1));
    lua_pop(lua, 1);
    if (plugin && *plugin)
    {
        delete *plugin;
        *plugin = nullptr;
    }
    return 0;
}

void ensureMetatables(lua_State *lua)
{
    if (luaL_newmetatable(lua, PLUGIN_METATABLE))
    {
        lua_pushcfunction(lua, onPluginGc);
        lua_setfield(lua, -2, "__gc");
    }
    lua_pop(lua, 1);
    if (luaL_newmetatable(lua, ATTACHMENT_METATABLE))
    {
        lua_pushcfunction(lua, onAttachmentGc);
        lua_setfield(lua, -2, "__gc");
    }
    lua_pop(lua, 1);
}

int openAttachments(lua_State *lua)
{
    ensureMetatables(lua);
    lua_newtable(lua);
    const int module = lua_absindex(lua, -1);
    auto **userdata = static_cast<ATTACHMENTS_PLUGIN **>(
        lua_newuserdata(lua, sizeof(ATTACHMENTS_PLUGIN *)));
    *userdata = new ATTACHMENTS_PLUGIN(lua);
    mbm::plugin_stamp_userdata(lua, &PLUGIN_IDENTIFIER);
    lua_rawseti(lua, module, 1);
    lua_pushlightuserdata(lua, *userdata);
    lua_pushcclosure(lua, onAttach, 1);
    lua_setfield(lua, module, "attach");
    luaL_getmetatable(lua, PLUGIN_METATABLE);
    lua_setmetatable(lua, module);
    mbm::plugin_doSubscribe(lua, module, "attachments");
    return 1;
}

}

extern "C" ATTACHMENTS_API int luaopen_attachments(lua_State *lua)
{
    return openAttachments(lua);
}

extern "C" ATTACHMENTS_API int luaopen_libattachments(lua_State *lua)
{
    return luaopen_attachments(lua);
}
