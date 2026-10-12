# Lua plugin symbol index

Companion to [the Lua API reference](lua-api.md). This inventory covers the `luaL_Reg`
method tables in all seven bundled Lua plugins, including build-conditional entries.
It records **registered names**, not the whole upstream C++ APIs. It is a navigation and
coverage index, not a claim that every method has a complete usage guide or runtime test.
Internal `__*` metamethods are excluded. Constants and dynamically constructed data tables
are outside this method inventory.

Checked against the repository bindings on 2026-10-12. When updating a registration
table, update this index and the relevant usage section together. Each binding link
points to its implementation; table names distinguish factory, object and module APIs.

Call conventions: physics world/joint/fluid and tilemap methods use `:`; ImGui and Steam
module functions use `.`. SQLite module functions use `.`, while database, statement,
callback-context and backup objects use `:`. `new` belongs to the factory; it is not an
ImGui module operation.

SQLite conditions: `temp_directory` is unavailable on Windows; `update_hook`,
`commit_hook`, and `rollback_hook` depend on `LSQLITE_OMIT_UPDATE_HOOK`. Other entries
may depend on plugin/build configuration. See the source preprocessor guards.

## Contents

- [Box2D world and factory](#box2d-world-and-factory)
- [Box2D joints](#box2d-joints)
- [LiquidFun world and factory](#liquidfun-world-and-factory)
- [LiquidFun joints](#liquidfun-joints)
- [LiquidFun fluids](#liquidfun-fluids)
- [ImGui](#imgui)
- [SQLite](#sqlite)
- [Bullet3D](#bullet3d)
- [Tilemap editor](#tilemap-editor)
- [Steam](#steam)

## Box2D world and factory

Source: [plugins/box2d/physics-box-2d-lua.cpp](../plugins/box2d/physics-box-2d-lua.cpp).

### `regBox2dMethods`

| Lua name | Binding |
|---|---|
| `addStaticBody` | [`onAddStaticBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L145) |
| `addDynamicBody` | [`onAddDynamicBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L274) |
| `addKinematicBody` | [`onAddKinematicBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L323) |
| `addBody` | [`onAddBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L190) |
| `applyForce` | [`onApplyForceBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L369) |
| `applyForceToCenter` | [`onApplyForceToCenterBodyBox2dFromBody`](../plugins/box2d/physics-box-2d-lua.cpp#L390) |
| `applyTorque` | [`onApplyTorqueBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L763) |
| `applyAngularImpulse` | [`onApplyAngularImpulseBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L801) |
| `applyLinearImpulse` | [`onApplyLinearImpulseBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L773) |
| `applyLinearImpulseToCenter` | [`onApplyLinearImpulseToCenterBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L792) |
| `destroyBody` | [`onDestroyBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1360) |
| `destroyJoint` | [`onDestroyJointBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1371) |
| `getAngularVelocity` | [`onGetAngularVelocityBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L428) |
| `getInertia` | [`onGetInertiaBox2dFromBody`](../plugins/box2d/physics-box-2d-lua.cpp#L435) |
| `getJoint` | [`onGetJointBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1332) |
| `getLinearVelocity` | [`onGetLinearVelocityBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L410) |
| `getGravity` | [`onGetGravityBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L129) |
| `getGravityScale` | [`onGetGravityScaleBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L138) |
| `getPosition` | [`onGetPositionBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L754) |
| `getMass` | [`onGetMassBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L442) |
| `getManifolds` | [`onGetManifoldBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L558) |
| `getWorldManifolds` | [`onGetWorldManifoldBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L605) |
| `getScale` | [`onGetScaleBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1047) |
| `getLocalCenter` | [`onGetLocalCenterBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1091) |
| `getLocalPoint` | [`onGetLocalPointBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1100) |
| `getType` | [`onGetTypeBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1111) |
| `getWorldCenter` | [`onGetWorldCenterBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1135) |
| `getWorldPoint` | [`onGetWorldPointBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1144) |
| `getWorldVector` | [`onGetWorldVectorBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1161) |
| `setTransform` | [`onInterfereBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L908) |
| `isActive` | [`onIsActiveBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L926) |
| `isAwake` | [`onIsAwakeBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L944) |
| `isOnTheGround` | [`onIsOnTheGroundBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L811) |
| `joint` | [`onCreateJointBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1466) |
| `pause` | [`onStopSimulateBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1346) |
| `queryAABB` | [`onQueryAABBBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1317) |
| `rayCast` | [`onRayCastBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1246) |
| `setActive` | [`onSetActiveCollisionBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L968) |
| `setAngularVelocity` | [`onSetAngularVelocityBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L420) |
| `setAngularDamping` | [`onSetAngularDumpingBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1039) |
| `setAwake` | [`onSetAwakeBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L936) |
| `setBullet` | [`onSetBulletBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L951) |
| `setContactListener` | [`onSetContactListenerBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L998) |
| `setDensity` | [`onSetDensityBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L897) |
| `setEnable` | [`onSetEnabledBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L959) |
| `setFilter` | [`onSetFilterBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1413) |
| `setFixedRotation` | [`onSetFixedRotationBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1395) |
| `setSleepingAllowed` | [`onSetSleepingAllowedBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1404) |
| `setGravity` | [`onSetGravityBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L113) |
| `setGravityScale` | [`onSetGravityScaleBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L121) |
| `setFriction` | [`onSetFrictionBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L819) |
| `setMass` | [`onSetMassBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L887) |
| `setManifolds` | [`onSetManifoldBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L624) |
| `setLinearVelocity` | [`onSetLinearVelocityBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L400) |
| `setRestitution` | [`onSetRestitutionBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L830) |
| `setScale` | [`onSetScaleBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1054) |
| `setMultiply` | [`onSetMultiplyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1083) |
| `getMultiply` | [`onGetMultiplyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1062) |
| `getVelocityIterations` | [`onGetVelocityIterationsBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1069) |
| `getPositionIterations` | [`onGetPositionIterationsBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1076) |
| `start` | [`onResumeSimulateBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1353) |
| `setType` | [`onSetTypeBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L841) |
| `testPoint` | [`onTestPointBodyBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1383) |

### `regBox2dMMethods`

| Lua name | Binding |
|---|---|
| `new` | [`onNewBox2dLua`](../plugins/box2d/physics-box-2d-lua.cpp#L1756) |
| `getVersion` | [`getVersionBox2d`](../plugins/box2d/physics-box-2d-lua.cpp#L1155) |

## Box2D joints

Source: [plugins/box2d/physics-box2d-joint-lua.cpp](../plugins/box2d/physics-box2d-joint-lua.cpp).

### `regMethods`

| Lua name | Binding |
|---|---|
| `getReactionForce` | [`onGetReactionForceJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L70) |
| `getReactionTorque` | [`onGetReactionTorqueJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L80) |
| `isActive` | [`onIsActiveJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L89) |
| `setActive` | [`onSetActiveJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L99) |
| `getMotorSpeed` | [`onGetMotorSpeedJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L143) |
| `setMotorSpeed` | [`onSetMotorSpeedJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L110) |
| `getMaxMotorTorque` | [`onGetMaxMotorTorqueJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L221) |
| `setMaxMotorTorque` | [`onSetMaxMotorTorqueJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L185) |
| `getDamping` | [`onGetRatioJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L263) |
| `setDamping` | [`onSetRatioJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L314) |
| `setMaxForce` | [`onSetMaxForceJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L357) |
| `getMaxForce` | [`onGetMaxForceJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L400) |
| `setStiffness` | [`onSetFrequencyHzJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L451) |
| `getStiffness` | [`onGetFrequencyHzJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L487) |
| `getLength` | [`onGetLengthJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L551) |
| `setLength` | [`onSetLengthJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L529) |
| `getTarget` | [`onGetTargetJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L599) |
| `setTarget` | [`onSetTargetJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L575) |
| `enableMotor` | [`onEnableMotorJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L649) |
| `enableLimit` | [`onEnableLimitJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L795) |
| `getCorrectionFactor` | [`onGetCorrectionFactorJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L702) |
| `setCorrectionFactor` | [`onSetCorrectionFactorJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L685) |
| `getLinearOffset` | [`onGetLinearOffsetJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L741) |
| `setLinearOffset` | [`onSetLinearOffsetJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L720) |
| `getAngularOffset` | [`onGetAngularOffsetJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L777) |
| `setAngularOffset` | [`onSetAngularOffsetJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L760) |
| `getLimits` | [`onGetLimitsJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L854) |
| `setLimits` | [`onSetLimitsJointBox2d`](../plugins/box2d/physics-box2d-joint-lua.cpp#L824) |
| `getAnchorA` | [`onGetAnchorA`](../plugins/box2d/physics-box2d-joint-lua.cpp#L631) |
| `getAnchorB` | [`onGetAnchorB`](../plugins/box2d/physics-box2d-joint-lua.cpp#L640) |

## LiquidFun world and factory

Source: [plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp).

### `regBox2dMethods`

| Lua name | Binding |
|---|---|
| `addStaticBody` | [`onAddStaticBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L144) |
| `addDynamicBody` | [`onAddDynamicBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L273) |
| `addKinematicBody` | [`onAddKinematicBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L635) |
| `createFluid` | [`onCreateFluidBodyBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L432) |
| `addBody` | [`onAddBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L189) |
| `applyForce` | [`onApplyForceBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L681) |
| `applyForceToCenter` | [`onApplyForceToCenterBodyBox2dlfFromBody`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L702) |
| `applyTorque` | [`onApplyTorqueBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1075) |
| `applyAngularImpulse` | [`onApplyAngularImpulseBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1113) |
| `applyLinearImpulse` | [`onApplyLinearImpulseBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1085) |
| `applyLinearImpulseToCenter` | [`onApplyLinearImpulseToCenterBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1104) |
| `destroyBody` | [`onDestroyBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1675) |
| `destroyJoint` | [`onDestroyJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1686) |
| `destroyFluid` | [`onDestroyFluidBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1698) |
| `getAngularVelocity` | [`onGetAngularVelocityBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L740) |
| `getInertia` | [`onGetInertiaBox2dFromBodylf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L747) |
| `getJoint` | [`onGetJointBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1647) |
| `getLinearVelocity` | [`onGetLinearVelocityBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L722) |
| `getGravity` | [`onGetGravityBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L128) |
| `getGravityScale` | [`onGetGravityScaleBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L137) |
| `getPosition` | [`onGetPositionBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1066) |
| `getMass` | [`onGetMassBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L754) |
| `getManifolds` | [`onGetManifoldBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L870) |
| `getWorldManifolds` | [`onGetWorldManifoldBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L917) |
| `getScale` | [`onGetScaleBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1360) |
| `getLocalCenter` | [`onGetLocalCenterBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1404) |
| `getLocalPoint` | [`onGetLocalPointBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1413) |
| `getType` | [`onGetTypeBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1424) |
| `getWorldCenter` | [`onGetWorldCenterBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1448) |
| `getWorldPoint` | [`onGetWorldPointBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1457) |
| `getWorldVector` | [`onGetWorldVectorBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1474) |
| `setTransform` | [`onInterfereBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1221) |
| `isActive` | [`onIsActiveBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1239) |
| `isAwake` | [`onIsAwakeBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1257) |
| `isOnTheGround` | [`onIsOnTheGroundBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1123) |
| `joint` | [`onCreateJointBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1792) |
| `pause` | [`onStopSimulateBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1661) |
| `queryAABB` | [`onQueryAABBBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1632) |
| `rayCast` | [`onRayCastBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1559) |
| `setActive` | [`onSetActiveCollisionBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1281) |
| `setAngularVelocity` | [`onSetAngularVelocityBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L732) |
| `setAngularDamping` | [`onSetAngularDumpingBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1352) |
| `setAwake` | [`onSetAwakeBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1249) |
| `setBullet` | [`onSetBulletBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1264) |
| `setContactListener` | [`onSetContactListenerBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1311) |
| `setDensity` | [`onSetDensityBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1210) |
| `setEnable` | [`onSetEnabledBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1272) |
| `setFilter` | [`onSetFilterBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1739) |
| `setFixedRotation` | [`onSetFixedRotationBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1721) |
| `setSleepingAllowed` | [`onSetSleepingAllowedBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1730) |
| `setGravity` | [`onSetGravityBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L112) |
| `setGravityScale` | [`onSetGravityScaleBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L120) |
| `setFriction` | [`onSetFrictionBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1131) |
| `setMass` | [`onSetMassBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1199) |
| `setManifolds` | [`onSetManifoldBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L936) |
| `setLinearVelocity` | [`onSetLinearVelocityBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L712) |
| `setRestitution` | [`onSetRestitutionBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1142) |
| `setScale` | [`onSetScaleBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1367) |
| `setMultiply` | [`onSetMultiplyBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1396) |
| `getMultiply` | [`onGetMultiplyBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1375) |
| `getVelocityIterations` | [`onGetVelocityIterationsBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1382) |
| `getPositionIterations` | [`onGetPositionIterationsBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1389) |
| `start` | [`onResumeSimulateBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1668) |
| `setType` | [`onSetTypeBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1153) |
| `testPoint` | [`onTestPointBodyBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1709) |

### `regBox2dMMethods`

| Lua name | Binding |
|---|---|
| `new` | [`onNewBox2dLua`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L2070) |
| `getVersion` | [`getVersionBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp#L1468) |

## LiquidFun joints

Source: [plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp).

### `regMethods`

| Lua name | Binding |
|---|---|
| `getReactionForce` | [`onGetReactionForceJointBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L70) |
| `getReactionTorque` | [`onGetReactionTorqueJointBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L80) |
| `isActive` | [`onIsActiveJointBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L89) |
| `setActive` | [`onSetActiveJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L99) |
| `getMotorSpeed` | [`onGetMotorSpeedJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L143) |
| `setMotorSpeed` | [`onSetMotorSpeedJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L110) |
| `getMaxMotorTorque` | [`onGetMaxMotorTorqueJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L221) |
| `setMaxMotorTorque` | [`onSetMaxMotorTorqueJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L185) |
| `getDampingRatio` | [`onGetRatioJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L263) |
| `setDampingRatio` | [`onSetRatioJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L314) |
| `setMaxForce` | [`onSetMaxForceJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L357) |
| `getMaxForce` | [`onGetMaxForceJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L400) |
| `setFrequencyHz` | [`onSetFrequencyHzJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L451) |
| `getFrequencyHz` | [`onGetFrequencyHzJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L487) |
| `getLength` | [`onGetLengthJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L558) |
| `setLength` | [`onSetLengthJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L529) |
| `getTarget` | [`onGetTargetJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L615) |
| `setTarget` | [`onSetTargetJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L591) |
| `enableMotor` | [`onEnableMotorJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L665) |
| `enableLimit` | [`onEnableLimitJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L811) |
| `getCorrectionFactor` | [`onGetCorrectionFactorJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L718) |
| `setCorrectionFactor` | [`onSetCorrectionFactorJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L701) |
| `getLinearOffset` | [`onGetLinearOffsetJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L757) |
| `setLinearOffset` | [`onSetLinearOffsetJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L736) |
| `getAngularOffset` | [`onGetAngularOffsetJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L793) |
| `setAngularOffset` | [`onSetAngularOffsetJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L776) |
| `getLimits` | [`onGetLimitsJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L870) |
| `setLimits` | [`onSetLimitsJointBox2d`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L840) |
| `getAnchorA` | [`onGetAnchorA`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L647) |
| `getAnchorB` | [`onGetAnchorB`](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp#L656) |

## LiquidFun fluids

Source: [plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp).

### `regMethods`

| Lua name | Binding |
|---|---|
| `add` | [`onAddParticleFluidBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L857) |
| `getParticleCount` | [`onGetParticleCountBox2dlf`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L53) |
| `getMaxParticleCount` | [`onGetMaxParticleCountBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L61) |
| `setMaxParticleCount` | [`onSetMaxParticleCountBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L69) |
| `setColor` | [`onSetColorFluidParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L801) |
| `getColor` | [`onGetColorFluidParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L839) |
| `setPaused` | [`onSetPausedParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L77) |
| `getDensity` | [`onGetDensityParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L93) |
| `setDensity` | [`onSetDensityParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L85) |
| `getGravityScale` | [`onGetGravityScaleParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L110) |
| `setGravityScale` | [`onSetGravityScaleParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L102) |
| `getDamping` | [`onGetDampingParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L119) |
| `setDamping` | [`onSetDampingParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L127) |
| `getStaticPressureIterations` | [`onGetStaticPressureIterationsParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L135) |
| `setStaticPressureIterations` | [`onSetStaticPressureIterationsParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L151) |
| `getRadius` | [`onGetRadiusParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L159) |
| `getParticleLifetime` | [`onGetParticleLifetimeParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L167) |
| `setParticleLifetime` | [`onSetParticleLifetimeParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L183) |
| `destroyParticle` | [`onDestroyParticleParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L195) |
| `destroyParticlesInShape` | [`onDestroyParticlesInShapeBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L206) |
| `particleApplyLinearImpulse` | [`onParticleApplyLinearImpulseBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L335) |
| `applyLinearImpulse` | [`onApplyLinearImpulseBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L373) |
| `particleApplyForce` | [`onParticleApplyForceBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L396) |
| `applyForce` | [`onApplyForceParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L433) |
| `queryAABB` | [`onQueryAABBParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L522) |
| `queryShapeAABB` | [`onQueryShapeAABBParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L551) |
| `computeAABB` | [`onComputeAABBParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L688) |
| `rayCast` | [`onRayCastParticleBox2d`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L775) |
| `getShader` | [`onGetShaderTableRenderizableLuaNoGC`](../plugins/box2d-liquid-fun-lua/physics-box-2d-fluid-lua.cpp#L922) |

## ImGui

Source: [plugins/imGui/imgui-lua.cpp](../plugins/imGui/imgui-lua.cpp).

### `regimguiMethods`

| Lua name | Binding |
|---|---|
| `AlignTextToFramePadding` | [`onAlignTextToFramePaddingImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4178) |
| `ArrowButton` | [`onArrowButtonImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4337) |
| `Begin` | [`onBeginImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3279) |
| `BeginChild` | [`onBeginChildImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3308) |
| `BeginDisabled` | [`onBeginDisabledImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3327) |
| `EndDisabled` | [`onEndDisabledImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3334) |
| `BeginCombo` | [`onBeginComboImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4536) |
| `BeginGroup` | [`onBeginGroupImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4088) |
| `BeginMainMenuBar` | [`onBeginMainMenuBarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5413) |
| `BeginMenu` | [`onBeginMenuImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5428) |
| `BeginMenuBar` | [`onBeginMenuBarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5398) |
| `BeginPopup` | [`onBeginPopupImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5577) |
| `BeginPopupContextItem` | [`onBeginPopupContextItemImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5589) |
| `BeginPopupContextVoid` | [`onBeginPopupContextVoidImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5613) |
| `BeginPopupContextWindow` | [`onBeginPopupContextWindowImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5601) |
| `BeginPopupModal` | [`onBeginPopupModalImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5625) |
| `BeginTable` | [`onBeginTableImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3341) |
| `BeginTabBar` | [`onBeginTabBarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5715) |
| `BeginTabItem` | [`onBeginTabItemImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5734) |
| `BeginTooltip` | [`onBeginTooltipImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5552) |
| `Bullet` | [`onBulletImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4529) |
| `BulletText` | [`onBulletTextImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4295) |
| `Button` | [`onButtonImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4304) |
| `CalcItemWidth` | [`onCalcItemWidthImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3984) |
| `CalcTextSize` | [`onCalcTextSizeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6018) |
| `CaptureKeyboardFromApp` | [`onCaptureKeyboardFromAppImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6245) |
| `CaptureMouseFromApp` | [`onCaptureMouseFromAppImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6447) |
| `GetWantCaptureMouse` | [`onGetWantCaptureMouseImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6256) |
| `GetWantCaptureKeyboard` | [`onGetWantCaptureKeyboardImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6270) |
| `Checkbox` | [`onCheckboxImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4485) |
| `CheckboxFlags` | [`onCheckboxFlagsImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4495) |
| `CloseCurrentPopup` | [`onCloseCurrentPopupImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5706) |
| `CollapsingHeader` | [`onCollapsingHeaderImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5244) |
| `ColorButton` | [`onColorButtonImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5148) |
| `ColorConvertFloat4ToU32` | [`onColorConvertFloat4ToU32ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6049) |
| `ColorConvertHSVtoRGB` | [`onColorConvertHSVtoRGBImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6074) |
| `ColorConvertRGBtoHSV` | [`onColorConvertRGBtoHSVImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6058) |
| `ColorConvertU32ToFloat4` | [`onColorConvertU32ToFloat4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6040) |
| `ColorEdit3` | [`onColorEdit3ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5087) |
| `ColorEdit4` | [`onColorEdit4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5101) |
| `ColorPicker3` | [`onColorPicker3ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5115) |
| `ColorPicker4` | [`onColorPicker4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5130) |
| `Combo` | [`onComboImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4556) |
| `DragFloat` | [`onDragFloatImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4588) |
| `DragFloat2` | [`onDragFloat2ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4605) |
| `DragFloat3` | [`onDragFloat3ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4623) |
| `DragFloat4` | [`onDragFloat4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4641) |
| `DragFloatRange2` | [`onDragFloatRange2ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4659) |
| `DragInt` | [`onDragIntImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4679) |
| `DragInt2` | [`onDragInt2ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4695) |
| `DragInt3` | [`onDragInt3ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4712) |
| `DragInt4` | [`onDragInt4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4729) |
| `DragIntRange2` | [`onDragIntRange2ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4746) |
| `Dummy` | [`onDummyImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4059) |
| `End` | [`onEndImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3302) |
| `EndChild` | [`onEndChildImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3321) |
| `EndCombo` | [`onEndComboImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4548) |
| `EndGroup` | [`onEndGroupImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4095) |
| `EndMainMenuBar` | [`onEndMainMenuBarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5421) |
| `EndMenu` | [`onEndMenuImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5440) |
| `EndMenuBar` | [`onEndMenuBarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5406) |
| `EndPopup` | [`onEndPopupImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5650) |
| `EndTable` | [`onEndTableImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3355) |
| `EndTabBar` | [`onEndTabBarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5727) |
| `EndTabItem` | [`onEndTabItemImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5760) |
| `EndTooltip` | [`onEndTooltipImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5560) |
| `Flags` | [`onMakeFlagsImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5462) |
| `FlagList` | [`onListFlagsImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5505) |
| `GetClipboardText` | [`onGetClipboardTextImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6458) |
| `GetColorU32` | [`onGetColorU32ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3948) |
| `GetContentRegionAvail` | [`onGetContentRegionAvailImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3707) |
| `GetCursorPos` | [`onGetCursorPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4102) |
| `GetCursorPosX` | [`onGetCursorPosXImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4110) |
| `GetCursorPosY` | [`onGetCursorPosYImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4118) |
| `GetCursorScreenPos` | [`onGetCursorScreenPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4161) |
| `GetCursorStartPos` | [`onGetCursorStartPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4153) |
| `GetItemRectMax` | [`onGetItemRectMaxImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5981) |
| `GetItemRectMin` | [`onGetItemRectMinImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5973) |
| `GetItemRectSize` | [`onGetItemRectSizeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5989) |
| `GetKeyPressedAmount` | [`onGetKeyPressedAmountImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6205) |
| `GetMainMenuBarHeight` | [`onGetMainMenuBarHeightImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6236) |
| `GetMouseCursor` | [`onGetMouseCursorImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6404) |
| `GetMouseDragDelta` | [`onGetMouseDragDeltaImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6382) |
| `GetMousePos` | [`onGetMousePosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6354) |
| `GetMousePosOnOpeningCurrentPopup` | [`onGetMousePosOnOpeningCurrentPopupImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6362) |
| `GetScrollMaxX` | [`onGetScrollMaxXImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3733) |
| `GetScrollMaxY` | [`onGetScrollMaxYImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3752) |
| `GetScrollX` | [`onGetScrollXImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3717) |
| `GetScrollY` | [`onGetScrollYImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3725) |
| `GetStyle` | [`onGetStyleImGuiLua`](../plugins/imGui/imgui-lua.cpp#L2883) |
| `GetStyleColorVec4` | [`onGetStyleColorVec4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3937) |
| `GetTextLineHeight` | [`onGetTextLineHeightImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4185) |
| `GetTextLineHeightWithSpacing` | [`onGetTextLineHeightWithSpacingImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4193) |
| `GetTreeNodeToLabelSpacing` | [`onGetTreeNodeToLabelSpacingImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5236) |
| `GetVersion` | [`onGetVersionImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3241) |
| `GetWindowHeight` | [`onGetWindowHeightImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3570) |
| `GetWindowPos` | [`onGetWindowPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3537) |
| `GetWindowSize` | [`onGetWindowSizeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3545) |
| `GetWindowWidth` | [`onGetWindowWidthImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3553) |
| `GetZoom` | [`onGetZoomImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3561) |
| `HelpMarker` | [`onHelpMarkerLua`](../plugins/imGui/imgui-lua.cpp#L6569) |
| `Image` | [`onImageImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4378) |
| `ImageQuad` | [`onImageQuadImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4415) |
| `ImageButton` | [`onImageButtonImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4454) |
| `Indent` | [`onIndentImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4068) |
| `InputDouble` | [`onInputDoubleImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5071) |
| `InputFloat` | [`onInputFloatImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4998) |
| `InputFloat2` | [`onInputFloat2ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6510) |
| `InputFloat3` | [`onInputFloat3ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6525) |
| `InputFloat4` | [`onInputFloat4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6540) |
| `InputInt` | [`onInputIntImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5014) |
| `InputInt2` | [`onInputInt2ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5029) |
| `InputInt3` | [`onInputInt3ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5043) |
| `InputInt4` | [`onInputInt4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5057) |
| `InputText` | [`onInputTextImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4950) |
| `InputTextMultiline` | [`onInputTextMultilineImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4966) |
| `InputTextWithHint` | [`onInputTextWithHintImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4982) |
| `InvisibleButton` | [`onInvisibleButtonImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4326) |
| `IsAnyItemActive` | [`onIsAnyItemActiveImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5957) |
| `IsAnyItemFocused` | [`onIsAnyItemFocusedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5965) |
| `IsAnyItemHovered` | [`onIsAnyItemHoveredImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5949) |
| `IsAnyMouseDown` | [`onIsAnyMouseDownImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6346) |
| `IsAnyWindowFocused` | [`onIsAnyWindowFocusedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6555) |
| `IsAnyWindowHovered` | [`onIsAnyWindowHoveredImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6562) |
| `IsItemActivated` | [`onIsItemActivatedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5917) |
| `IsItemActive` | [`onIsItemActiveImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5874) |
| `IsItemClicked` | [`onIsItemClickedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5890) |
| `IsItemDeactivated` | [`onIsItemDeactivatedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5925) |
| `IsItemDeactivatedAfterChange` | [`onIsItemDeactivatedAfterChangeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6503) |
| `IsItemDeactivatedAfterEdit` | [`onIsItemDeactivatedAfterEditImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5933) |
| `IsItemEdited` | [`onIsItemEditedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5909) |
| `IsItemFocused` | [`onIsItemFocusedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5882) |
| `IsItemHovered` | [`onIsItemHoveredImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5863) |
| `IsItemToggledOpen` | [`onIsItemToggledOpenImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5941) |
| `IsItemVisible` | [`onIsItemVisibleImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5901) |
| `IsKeyDown` | [`onIsKeyDownImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6092) |
| `IsKeyPressed` | [`onIsKeyPressedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6121) |
| `IsKeyReleased` | [`onIsKeyReleasedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6176) |
| `IsMouseClicked` | [`onIsMouseClickedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6288) |
| `IsMouseDoubleClicked` | [`onIsMouseDoubleClickedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6310) |
| `IsMouseDown` | [`onIsMouseDownImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6278) |
| `IsMouseDragging` | [`onIsMouseDraggingImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6370) |
| `IsMouseHoveringRect` | [`onIsMouseHoveringRectImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6320) |
| `IsMousePosValid` | [`onIsMousePosValidImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6335) |
| `IsMouseReleased` | [`onIsMouseReleasedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6300) |
| `IsPopupOpen` | [`onIsPopupOpenImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5674) |
| `IsRectVisible` | [`onIsRectVisibleImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6004) |
| `IsScrollVisible` | [`onIsScrollVisibleImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3742) |
| `IsWindowAppearing` | [`onIsWindowAppearingImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3501) |
| `IsWindowCollapsed` | [`onIsWindowCollapsedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3508) |
| `IsWindowFocused` | [`onIsWindowFocusedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3515) |
| `IsWindowHovered` | [`onIsWindowHoveredImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3526) |
| `LabelText` | [`onLabelTextImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4285) |
| `BeginListBox` | [`onBeginListBoxImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5330) |
| `EndListBox` | [`onEndListBoxImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5343) |
| `ListBox` | [`onListBoxImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5307) |
| `LogButtons` | [`onLogButtonsImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5814) |
| `LogFinish` | [`onLogFinishImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5807) |
| `LogText` | [`onLogTextImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5821) |
| `LogToClipboard` | [`onLogToClipboardImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5797) |
| `LogToFile` | [`onLogToFileImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5786) |
| `LogToTTY` | [`onLogToTTYImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5776) |
| `MenuItem` | [`onMenuItemImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5447) |
| `NewLine` | [`onNewLineImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4045) |
| `OpenPopup` | [`onOpenPopupImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5568) |
| `OpenPopupOnItemClick` | [`onOpenPopupOnItemClickImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5657) |
| `PlotHistogram` | [`onPlotHistogramImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5374) |
| `PlotLines` | [`onPlotLinesImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5350) |
| `PopItemFlag` | [`onPopItemFlagImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4020) |
| `PopClipRect` | [`onPopClipRectImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5840) |
| `PopFont` | [`onPopFontImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3820) |
| `PopID` | [`onPopIDImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4240) |
| `PopItemWidth` | [`onPopItemWidthImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3969) |
| `PopStyleColor` | [`onPopStyleColorImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3872) |
| `PopStyleVar` | [`onPopStyleVarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3928) |
| `PopTextWrapPos` | [`onPopTextWrapPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4002) |
| `ProgressBar` | [`onProgressBarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4518) |
| `PushItemFlag` | [`onPushItemFlagImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4010) |
| `PushClipRect` | [`onPushClipRectImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5830) |
| `PushID` | [`onPushIDImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4203) |
| `PushItemWidth` | [`onPushItemWidthImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3960) |
| `PushStyleColor` | [`onPushStyleColorImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3826) |
| `PushStyleVar` | [`onPushStyleVarImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3881) |
| `PushTextWrapPos` | [`onPushTextWrapPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3992) |
| `RadioButton` | [`onRadioButtonImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4507) |
| `ResetMouseDragDelta` | [`onResetMouseDragDeltaImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6394) |
| `SameLine` | [`onSameLineImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4034) |
| `Selectable` | [`onSelectableImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5292) |
| `Separator` | [`onSeparatorImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4027) |
| `SetClipboardText` | [`onSetClipboardTextImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6465) |
| `SetColorEditOptions` | [`onSetColorEditOptionsImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5164) |
| `SetCursorPos` | [`onSetCursorPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4126) |
| `SetCursorPosX` | [`onSetCursorPosXImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4135) |
| `SetCursorPosY` | [`onSetCursorPosYImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4144) |
| `SetCursorScreenPos` | [`onSetCursorScreenPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4169) |
| `SetNextItemAllowOverlap` | [`onSetNextItemAllowOverlapImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5997) |
| `SetNextItemShortcut` | [`onSetNextItemShortcutImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5269) |
| `SetShortcut` | [`onShortcutImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6152) |
| `SetItemDefaultFocus` | [`onSetItemDefaultFocusImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5846) |
| `SetKeyboardFocusHere` | [`onSetKeyboardFocusHereImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5853) |
| `SetMouseCursor` | [`onSetMouseCursorImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6419) |
| `SetNextItemOpen` | [`onSetNextItemOpenImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5258) |
| `SetNextItemWidth` | [`onSetNextItemWidthImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3975) |
| `SetNextTreeNodeOpen` | [`onSetNextTreeNodeOpenImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6481) |
| `SetNextWindowBgAlpha` | [`onSetNextWindowBgAlphaImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3639) |
| `SetNextWindowCollapsed` | [`onSetNextWindowCollapsedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3621) |
| `SetNextWindowContentSize` | [`onSetNextWindowContentSizeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3612) |
| `SetNextWindowFocus` | [`onSetNextWindowFocusImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3632) |
| `SetNextWindowPos` | [`onSetNextWindowPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3578) |
| `SetNextWindowSize` | [`onSetNextWindowSizeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3590) |
| `SetNextWindowSizeConstraints` | [`onSetNextWindowSizeConstraintsImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3602) |
| `SetScrollFromPosX` | [`onSetScrollFromPosXImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3798) |
| `SetScrollFromPosY` | [`onSetScrollFromPosYImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3809) |
| `SetScrollHere` | [`onSetScrollHereImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6494) |
| `SetScrollHereX` | [`onSetScrollHereXImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3778) |
| `SetScrollHereY` | [`onSetScrollHereYImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3788) |
| `SetScrollX` | [`onSetScrollXImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3760) |
| `SetScrollY` | [`onSetScrollYImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3769) |
| `TableAngledHeadersRow` | [`onTableAngledHeadersRowImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3419) |
| `TableGetColumnCount` | [`onTableGetColumnCountImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3425) |
| `TableGetColumnFlags` | [`onTableGetColumnFlagsImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3454) |
| `TableGetColumnIndex` | [`onTableGetColumnIndexImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3432) |
| `TableGetColumnName` | [`onTableGetColumnNameImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3446) |
| `TableGetHoveredColumn` | [`onTableGetHoveredColumnImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3470) |
| `TableGetRowIndex` | [`onTableGetRowIndexImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3439) |
| `TableHeader` | [`onTableHeaderImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3406) |
| `TableHeadersRow` | [`onTableHeadersRowImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3413) |
| `TableNextColumn` | [`onTableNextColumnImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3371) |
| `TableNextRow` | [`onTableNextRowImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3361) |
| `TableSetBgColor` | [`onTableSetBgColorImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3477) |
| `TableSetColumnEnabled` | [`onTableSetColumnEnabledImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3462) |
| `TableSetColumnIndex` | [`onTableSetColumnIndexImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3378) |
| `TableSetupColumn` | [`onTableSetupColumnImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3386) |
| `TableSetupScrollFreeze` | [`onTableSetupScrollFreezeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3398) |
| `SetTabItemClosed` | [`onSetTabItemClosedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5767) |
| `SetTooltip` | [`onSetTooltipImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5543) |
| `SetWindowCollapsed` | [`onSetWindowCollapsedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3690) |
| `SetWindowFocus` | [`onSetWindowFocusImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3678) |
| `SetWindowPos` | [`onSetWindowPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3648) |
| `SetWindowSize` | [`onSetWindowSizeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3663) |
| `ShowDemoWindow` | [`onShowDemoWindowImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3206) |
| `ShowFontSelector` | [`onShowFontSelectorImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3223) |
| `ShowStyleSelector` | [`onShowStyleSelectorImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3213) |
| `ShowUserGuide` | [`onShowUserGuideImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3232) |
| `SliderAngle` | [`onSliderAngleImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4833) |
| `SliderFloat` | [`onSliderFloatImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4765) |
| `SliderFloat2` | [`onSliderFloat2ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4782) |
| `SliderFloat3` | [`onSliderFloat3ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4799) |
| `SliderFloat4` | [`onSliderFloat4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4816) |
| `SliderInt` | [`onSliderIntImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4849) |
| `SliderInt2` | [`onSliderInt2ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4865) |
| `SliderInt3` | [`onSliderInt3ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4882) |
| `SliderInt4` | [`onSliderInt4ImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4899) |
| `SmallButton` | [`onSmallButtonImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4316) |
| `Spacing` | [`onSpacingImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4052) |
| `StyleColorsClassic` | [`onStyleColorsClassicImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3259) |
| `StyleColorsDark` | [`onStyleColorsDarkImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3249) |
| `StyleColorsLight` | [`onStyleColorsLightImGuiLua`](../plugins/imGui/imgui-lua.cpp#L3269) |
| `Text` | [`onTextImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4249) |
| `TextColored` | [`onTextColoredImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4257) |
| `TextDisabled` | [`onTextDisabledImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4267) |
| `TextWrapped` | [`onTextWrappedImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4276) |
| `TreeAdvanceToLabelPos` | [`onTreeAdvanceToLabelPosImGuiLua`](../plugins/imGui/imgui-lua.cpp#L6475) |
| `TreeNode` | [`onTreeNodeImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5173) |
| `TreeNodeEx` | [`onTreeNodeExImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5200) |
| `TreePop` | [`onTreePopImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5229) |
| `TreePush` | [`onTreePushImGuiLua`](../plugins/imGui/imgui-lua.cpp#L5220) |
| `Unindent` | [`onUnindentImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4078) |
| `VSliderFloat` | [`onVSliderFloatImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4916) |
| `VSliderInt` | [`onVSliderIntImGuiLua`](../plugins/imGui/imgui-lua.cpp#L4933) |
| `AddBezierCubic` | [`onAddBezierCubicImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6933) |
| `AddCircle` | [`onAddCircleImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6725) |
| `AddCircleFilled` | [`onAddCircleFilledImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6739) |
| `AddConvexPolyFilled` | [`onAddConvexPolyFilledImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6921) |
| `AddDrawCmd` | [`onAddDrawCmdImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L7034) |
| `AddImage` | [`onAddImageImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6949) |
| `AddImageQuad` | [`onAddImageQuadImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6975) |
| `AddImageRounded` | [`onAddImageRoundedImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L7006) |
| `AddLine` | [`onAddLineImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6601) |
| `AddNgon` | [`onAddNgonImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6752) |
| `AddNgonFilled` | [`onAddNgonFilledImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6766) |
| `CreateGeometryBatch` | [`onCreateGeometryBatchLua`](../plugins/imGui/imgui-lua.cpp#L6811) |
| `AddGeometryBatch` | [`onAddGeometryBatchLua`](../plugins/imGui/imgui-lua.cpp#L6871) |
| `AddPolyline` | [`onAddPolylineImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6908) |
| `AddQuad` | [`onAddQuadImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6671) |
| `AddQuadFilled` | [`onAddQuadFilledImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6686) |
| `AddRect` | [`onAddRectImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6626) |
| `AddRectFilled` | [`onAddRectFilledImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6642) |
| `AddRectFilledMultiColor` | [`onAddRectFilledMultiColorImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6657) |
| `AddText` | [`onAddTextImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6778) |
| `AddTriangle` | [`onAddTriangleImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6699) |
| `AddTriangleFilled` | [`onAddTriangleFilledImDrawListLua`](../plugins/imGui/imgui-lua.cpp#L6713) |
| `SetImDrawListToBackground` | [`onImDrawListToBackgroundLua`](../plugins/imGui/imgui-lua.cpp#L6614) |
| `SetImDrawListToForeground` | [`onImDrawListToForegroundLua`](../plugins/imGui/imgui-lua.cpp#L6620) |

### `regimguiMethods`

| Lua name | Binding |
|---|---|
| `new` | [`onNewimguiLua`](../plugins/imGui/imgui-lua.cpp#L7042) |

## SQLite

Source: [third-party/lsqlite3/lsqlite3.c](../third-party/lsqlite3/lsqlite3.c).

### `dblib`

| Lua name | Binding |
|---|---|
| `isopen` | [`db_isopen`](../third-party/lsqlite3/lsqlite3.c#L896) |
| `last_insert_rowid` | [`db_last_insert_rowid`](../third-party/lsqlite3/lsqlite3.c#L902) |
| `changes` | [`db_changes`](../third-party/lsqlite3/lsqlite3.c#L910) |
| `total_changes` | [`db_total_changes`](../third-party/lsqlite3/lsqlite3.c#L916) |
| `errcode` | [`db_errcode`](../third-party/lsqlite3/lsqlite3.c#L922) |
| `error_code` | [`db_errcode`](../third-party/lsqlite3/lsqlite3.c#L922) |
| `errmsg` | [`db_errmsg`](../third-party/lsqlite3/lsqlite3.c#L928) |
| `error_message` | [`db_errmsg`](../third-party/lsqlite3/lsqlite3.c#L928) |
| `interrupt` | [`db_interrupt`](../third-party/lsqlite3/lsqlite3.c#L934) |
| `db_filename` | [`db_db_filename`](../third-party/lsqlite3/lsqlite3.c#L940) |
| `create_function` | [`db_create_function`](../third-party/lsqlite3/lsqlite3.c#L1166) |
| `create_aggregate` | [`db_create_aggregate`](../third-party/lsqlite3/lsqlite3.c#L1170) |
| `create_collation` | [`db_create_collation`](../third-party/lsqlite3/lsqlite3.c#L1201) |
| `load_extension` | [`db_load_extension`](../third-party/lsqlite3/lsqlite3.c#L1229) |
| `trace` | [`db_trace`](../third-party/lsqlite3/lsqlite3.c#L1279) |
| `progress_handler` | [`db_progress_handler`](../third-party/lsqlite3/lsqlite3.c#L1518) |
| `busy_timeout` | [`db_busy_timeout`](../third-party/lsqlite3/lsqlite3.c#L1734) |
| `busy_handler` | [`db_busy_handler`](../third-party/lsqlite3/lsqlite3.c#L1703) |
| `update_hook` | [`db_update_hook`](../third-party/lsqlite3/lsqlite3.c#L1343) |
| `commit_hook` | [`db_commit_hook`](../third-party/lsqlite3/lsqlite3.c#L1402) |
| `rollback_hook` | [`db_rollback_hook`](../third-party/lsqlite3/lsqlite3.c#L1457) |
| `prepare` | [`db_prepare`](../third-party/lsqlite3/lsqlite3.c#L1836) |
| `rows` | [`db_rows`](../third-party/lsqlite3/lsqlite3.c#L1967) |
| `urows` | [`db_urows`](../third-party/lsqlite3/lsqlite3.c#L1976) |
| `nrows` | [`db_nrows`](../third-party/lsqlite3/lsqlite3.c#L1971) |
| `exec` | [`db_exec`](../third-party/lsqlite3/lsqlite3.c#L1804) |
| `execute` | [`db_exec`](../third-party/lsqlite3/lsqlite3.c#L1804) |
| `close` | [`db_close`](../third-party/lsqlite3/lsqlite3.c#L1991) |
| `close_vm` | [`db_close_vm`](../third-party/lsqlite3/lsqlite3.c#L1997) |
| `get_ptr` | [`db_get_ptr`](../third-party/lsqlite3/lsqlite3.c#L2028) |

### `vmlib`

| Lua name | Binding |
|---|---|
| `isopen` | [`dbvm_isopen`](../third-party/lsqlite3/lsqlite3.c#L266) |
| `step` | [`dbvm_step`](../third-party/lsqlite3/lsqlite3.c#L290) |
| `reset` | [`dbvm_reset`](../third-party/lsqlite3/lsqlite3.c#L307) |
| `finalize` | [`dbvm_finalize`](../third-party/lsqlite3/lsqlite3.c#L302) |
| `columns` | [`dbvm_columns`](../third-party/lsqlite3/lsqlite3.c#L345) |
| `bind` | [`dbvm_bind`](../third-party/lsqlite3/lsqlite3.c#L535) |
| `bind_values` | [`dbvm_bind_values`](../third-party/lsqlite3/lsqlite3.c#L558) |
| `bind_names` | [`dbvm_bind_names`](../third-party/lsqlite3/lsqlite3.c#L582) |
| `bind_blob` | [`dbvm_bind_blob`](../third-party/lsqlite3/lsqlite3.c#L548) |
| `bind_parameter_count` | [`dbvm_bind_parameter_count`](../third-party/lsqlite3/lsqlite3.c#L521) |
| `bind_parameter_name` | [`dbvm_bind_parameter_name`](../third-party/lsqlite3/lsqlite3.c#L527) |
| `get_value` | [`dbvm_get_value`](../third-party/lsqlite3/lsqlite3.c#L357) |
| `get_values` | [`dbvm_get_values`](../third-party/lsqlite3/lsqlite3.c#L382) |
| `get_name` | [`dbvm_get_name`](../third-party/lsqlite3/lsqlite3.c#L366) |
| `get_names` | [`dbvm_get_names`](../third-party/lsqlite3/lsqlite3.c#L397) |
| `get_type` | [`dbvm_get_type`](../third-party/lsqlite3/lsqlite3.c#L374) |
| `get_types` | [`dbvm_get_types`](../third-party/lsqlite3/lsqlite3.c#L411) |
| `get_uvalues` | [`dbvm_get_uvalues`](../third-party/lsqlite3/lsqlite3.c#L425) |
| `get_unames` | [`dbvm_get_unames`](../third-party/lsqlite3/lsqlite3.c#L438) |
| `get_utypes` | [`dbvm_get_utypes`](../third-party/lsqlite3/lsqlite3.c#L450) |
| `get_named_values` | [`dbvm_get_named_values`](../third-party/lsqlite3/lsqlite3.c#L462) |
| `get_named_types` | [`dbvm_get_named_types`](../third-party/lsqlite3/lsqlite3.c#L478) |
| `rows` | [`dbvm_rows`](../third-party/lsqlite3/lsqlite3.c#L1935) |
| `urows` | [`dbvm_urows`](../third-party/lsqlite3/lsqlite3.c#L1943) |
| `nrows` | [`dbvm_nrows`](../third-party/lsqlite3/lsqlite3.c#L1939) |
| `last_insert_rowid` | [`dbvm_last_insert_rowid`](../third-party/lsqlite3/lsqlite3.c#L332) |
| `idata` | [`dbvm_get_values`](../third-party/lsqlite3/lsqlite3.c#L382) |
| `inames` | [`dbvm_get_names`](../third-party/lsqlite3/lsqlite3.c#L397) |
| `itypes` | [`dbvm_get_types`](../third-party/lsqlite3/lsqlite3.c#L411) |
| `data` | [`dbvm_get_named_values`](../third-party/lsqlite3/lsqlite3.c#L462) |
| `type` | [`dbvm_get_named_types`](../third-party/lsqlite3/lsqlite3.c#L478) |

### `ctxlib`

| Lua name | Binding |
|---|---|
| `user_data` | [`lcontext_user_data`](../third-party/lsqlite3/lsqlite3.c#L785) |
| `get_aggregate_data` | [`lcontext_get_aggregate_context`](../third-party/lsqlite3/lsqlite3.c#L792) |
| `set_aggregate_data` | [`lcontext_set_aggregate_context`](../third-party/lsqlite3/lsqlite3.c#L799) |
| `aggregate_count` | [`lcontext_aggregate_count`](../third-party/lsqlite3/lsqlite3.c#L808) |
| `result` | [`lcontext_result`](../third-party/lsqlite3/lsqlite3.c#L820) |
| `result_null` | [`lcontext_result_null`](../third-party/lsqlite3/lsqlite3.c#L876) |
| `result_number` | [`lcontext_result_double`](../third-party/lsqlite3/lsqlite3.c#L854) |
| `result_double` | [`lcontext_result_double`](../third-party/lsqlite3/lsqlite3.c#L854) |
| `result_int` | [`lcontext_result_int`](../third-party/lsqlite3/lsqlite3.c#L869) |
| `result_text` | [`lcontext_result_text`](../third-party/lsqlite3/lsqlite3.c#L882) |
| `result_blob` | [`lcontext_result_blob`](../third-party/lsqlite3/lsqlite3.c#L846) |
| `result_error` | [`lcontext_result_error`](../third-party/lsqlite3/lsqlite3.c#L861) |

### `dbbulib`

| Lua name | Binding |
|---|---|
| `step` | [`dbbu_step`](../third-party/lsqlite3/lsqlite3.c#L1651) |
| `remaining` | [`dbbu_remaining`](../third-party/lsqlite3/lsqlite3.c#L1658) |
| `pagecount` | [`dbbu_pagecount`](../third-party/lsqlite3/lsqlite3.c#L1664) |
| `finish` | [`dbbu_finish`](../third-party/lsqlite3/lsqlite3.c#L1670) |

### `sqlitelib`

| Lua name | Binding |
|---|---|
| `lversion` | [`lsqlite_lversion`](../third-party/lsqlite3/lsqlite3.c#L2155) |
| `version` | [`lsqlite_version`](../third-party/lsqlite3/lsqlite3.c#L2047) |
| `complete` | [`lsqlite_complete`](../third-party/lsqlite3/lsqlite3.c#L2052) |
| `temp_directory` | [`lsqlite_temp_directory`](../third-party/lsqlite3/lsqlite3.c#L2059) |
| `open` | [`lsqlite_open`](../third-party/lsqlite3/lsqlite3.c#L2108) |
| `open_memory` | [`lsqlite_open_memory`](../third-party/lsqlite3/lsqlite3.c#L2114) |
| `open_ptr` | [`lsqlite_open_ptr`](../third-party/lsqlite3/lsqlite3.c#L2123) |
| `backup_init` | [`lsqlite_backup_init`](../third-party/lsqlite3/lsqlite3.c#L1594) |

## Bullet3D

Source: [plugins/bullet3d/physics-bullet-3d-lua.cpp](../plugins/bullet3d/physics-bullet-3d-lua.cpp).

### `regBullet3dMethods`

| Lua name | Binding |
|---|---|
| `getGravity` | [`onGetGravityBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L237) |
| `setGravity` | [`onSetGravityBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L229) |
| `addStaticBody` | [`onAddStaticBodyBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L149) |
| `addDynamicBody` | [`onAddDynamicBodyBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L67) |
| `addKinematicBody` | [`onAddKinematicBodyBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L189) |
| `addDynamicCharacterBody` | [`onAddDynamicCharacterBodyBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L108) |
| `applyForce` | [`onApplyForceBodyBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L247) |
| `applyTorque` | [`onApplyTorqueBodyBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L260) |
| `applyImpulse` | [`onApplyImpulseBodyBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L273) |
| `setLinearVelocity` | [`onSetLinearVelocityBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L286) |
| `isOnTheGround` | [`onIsOnTheGroundBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L474) |
| `setFriction` | [`onSetFrictionBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L299) |
| `setRestituition` | [`onSetRestituitionBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L310) |
| `setMass` | [`onSetMassBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L321) |
| `interfere` | [`onInterfereBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L335) |
| `setAwake` | [`onSetAwakeBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L363) |
| `isAwake` | [`onIsAwakeBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L374) |
| `setContactListener` | [`onSetContactListenerBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L448) |
| `rayCast` | [`onRayCastBullet3d`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L484) |

### `regBullet3dMMethods`

| Lua name | Binding |
|---|---|
| `new` | [`onNewBullet3dLua`](../plugins/bullet3d/physics-bullet-3d-lua.cpp#L517) |

## Tilemap editor

Source: [plugins/tiled/tiled-lua.cpp](../plugins/tiled/tiled-lua.cpp).

### `regMethodsTileEditorMethods`

| Lua name | Binding |
|---|---|
| `getMapCountWidth` | [`onGetMapCountWidthTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L313) |
| `setMapCountWidth` | [`onSetMapCountWidthTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L319) |
| `getMapCountHeight` | [`onGetMapCountHeightTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L325) |
| `setMapCountHeight` | [`onSetMapCountHeightTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L331) |
| `getMapTileWidth` | [`onGetMapTileWidthTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L338) |
| `setMapTileWidth` | [`onSetMapTileWidthTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L344) |
| `getMapTileHeight` | [`onGetMapTileHeightTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L350) |
| `setMapTileHeight` | [`onSetMapTileHeightTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L356) |
| `getMapBackgroundColor` | [`onGetMapBackgroundColorTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L410) |
| `setMapBackgroundColor` | [`onSetMapBackgroundColorTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L416) |
| `getMapBackgroundTexture` | [`onGetMapBackgroundTextureTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L423) |
| `setMapBackgroundTexture` | [`onSetMapBackgroundTextureTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L433) |
| `getMapProperties` | [`onGetMapPropertiesTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L440) |
| `setMapProperty` | [`onSetMapPropertiesTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L448) |
| `setOverBrick` | [`onOverBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1463) |
| `selectBrick` | [`onSelectBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1472) |
| `unselectBrick` | [`onUnselectBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1527) |
| `isBrickSelected` | [`onIsBrickSelectedTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1510) |
| `getFirstSelectedBrick` | [`onGetFirstSelectedBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1519) |
| `getIndexTileIdOver` | [`onGetIndexOverBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1483) |
| `getSelectedTileIDs` | [`onGetSelectedBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1536) |
| `setBrickToLayer` | [`onSetBrick2LayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1555) |
| `rotate` | [`onRotateSelectedBrickLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1637) |
| `flip` | [`onFlipSelectedLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1646) |
| `getObjectMap` | [`onGetObjectMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1328) |
| `getTotalObjectMap` | [`onGetTotalObjectMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1278) |
| `setObjectMap` | [`onSetObjectMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1368) |
| `addObjectMap` | [`onAddObjectMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1286) |
| `deleteObjectMap` | [`onDeleteObjectMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1261) |
| `setMapType` | [`onSetMapTypeTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1697) |
| `getMapType` | [`onGetMapTypeTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1705) |
| `setDirectionMapRender` | [`onSetDirectionMapRenderTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1712) |
| `getDirectionMapRender` | [`onGetDirectionMapRenderTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1721) |
| `setMaxTileToRender` | [`onSetMaxTileToRenderTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1733) |
| `getMaxTileToRender` | [`onGetMaxTileToRenderTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1741) |
| `moveWholeLayerTo` | [`onMoveWholeLayerToTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1749) |
| `addHistoric` | [`onAddHistoricMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1668) |
| `unDo` | [`onUnDoMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1675) |
| `redDo` | [`onRedDoMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1682) |
| `clearHistory` | [`onClearHistoryMapTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1689) |
| `getTotalLayer` | [`onGetTotalLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1456) |
| `getLayer` | [`onGetLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L482) |
| `getNameShaderLayer` | [`onGetNameShaderLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L456) |
| `removeShaderFromLayer` | [`onRemoveShaderLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L469) |
| `newLayer` | [`onNewLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L521) |
| `updateLayer` | [`onUpdateLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L528) |
| `deleteLayer` | [`onDeleteLayerTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L594) |
| `getLayerProperties` | [`onGetLayerPropertiesTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L607) |
| `setLayerProperty` | [`onSetLayerPropertiesTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L630) |
| `selectAllBricks` | [`onSelectAllBricksTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1566) |
| `invertSelectedBricks` | [`onInvertSelectedBricksTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1573) |
| `unselectAllBricks` | [`onUnSelectBricksTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1580) |
| `deleteSelectedBricks` | [`onDeleteSelectedBricksTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1587) |
| `moveLayerUp` | [`onMoveLayerUpTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1594) |
| `moveLayerDown` | [`onMoveLayerDownTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1611) |
| `duplicateSelectedTiles` | [`onDuplicateSelectedTilesLua`](../plugins/tiled/tiled-lua.cpp#L1628) |
| `newTileSet` | [`onNewTileSetTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L696) |
| `getTotalTileSet` | [`onGetTotalTileSetTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L744) |
| `getTileSetName` | [`onGetTileSetNameTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L751) |
| `getTileSetWidth` | [`onGetTileSetWidthTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L363) |
| `setTileSetWidth` | [`onSetTileSetWidthTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L370) |
| `getTileSetHeight` | [`onGetTileSetHeightTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L377) |
| `setTileSetHeight` | [`onSetTileSetHeightTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L384) |
| `existTileSet` | [`onExistTileSetTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L392) |
| `setTileSetName` | [`onSetTileSetNameTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L400) |
| `getTotalBricks` | [`onGetTotalBricksTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L829) |
| `getBrick` | [`onGetBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L921) |
| `updateBrick` | [`onUpdateBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L942) |
| `undoChangesBrick` | [`onUndoChangesBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L954) |
| `expandBrick` | [`onExpandBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L969) |
| `expandVBrick` | [`onExpandVBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L987) |
| `expandHBrick` | [`onExpandHBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1005) |
| `getPhysicsBrick` | [`onGetPhysicsBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1178) |
| `setPhysicsBrick` | [`onSetPhysicsBrickTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1215) |
| `getBrickProperties` | [`onGetBrickPropertiesTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L651) |
| `getBrickID` | [`onGetBrickIDByTileIDLua`](../plugins/tiled/tiled-lua.cpp#L668) |
| `setBrickProperty` | [`onSetBrickPropertiesTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L679) |
| `setRenderMode` | [`onSetRenderModeTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L765) |
| `showTileSetPreview` | [`onShowTileSetPreviewTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1654) |
| `getScale` | [`onGetScaleTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1430) |
| `setScale` | [`onSetScaleTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L1443) |
| `load` | [`onLoadTiledEditorLua`](../plugins/tiled/tiled-lua.cpp#L264) |
| `save` | [`onSavetileEditor`](../plugins/tiled/tiled-lua.cpp#L279) |
| `selectTiles` | [`onSelectTilesLua`](../plugins/tiled/tiled-lua.cpp#L290) |
| `version` | [`onGetVersion`](../plugins/tiled/tiled-lua.cpp#L300) |

### `regMethodsTileEditorMethods`

| Lua name | Binding |
|---|---|
| `new` | [`onNewTileEditorLua`](../plugins/tiled/tiled-lua.cpp#L1759) |

## Steam

Source: [plugins/steam/steam-class-lua.cpp](../plugins/steam/steam-class-lua.cpp).

### `regMethods`

| Lua name | Binding |
|---|---|
| `isReady` | [`onSteamIsReadyLua`](../plugins/steam/steam-class-lua.cpp#L57) |
| `getPlayerName` | [`onSteamGetPlayerNameLua`](../plugins/steam/steam-class-lua.cpp#L65) |
| `getAppId` | [`onSteamGetAppIdLua`](../plugins/steam/steam-class-lua.cpp#L74) |
| `getCurrentLanguage` | [`onSteamGetCurrentLanguageLua`](../plugins/steam/steam-class-lua.cpp#L82) |
| `setAchievement` | [`onSteamSetAchievementLua`](../plugins/steam/steam-class-lua.cpp#L95) |
| `clearAchievement` | [`onSteamClearAchievementLua`](../plugins/steam/steam-class-lua.cpp#L105) |
| `isAchievementAchieved` | [`onSteamIsAchievementAchievedLua`](../plugins/steam/steam-class-lua.cpp#L115) |
| `setStatInt` | [`onSteamSetStatIntLua`](../plugins/steam/steam-class-lua.cpp#L130) |
| `setStatFloat` | [`onSteamSetStatFloatLua`](../plugins/steam/steam-class-lua.cpp#L141) |
| `getStatInt` | [`onSteamGetStatIntLua`](../plugins/steam/steam-class-lua.cpp#L152) |
| `getStatFloat` | [`onSteamGetStatFloatLua`](../plugins/steam/steam-class-lua.cpp#L163) |
| `storeStats` | [`onSteamStoreStatsLua`](../plugins/steam/steam-class-lua.cpp#L176) |
| `findLeaderboard` | [`onSteamFindLeaderboardLua`](../plugins/steam/steam-class-lua.cpp#L192) |
| `uploadScore` | [`onSteamUploadScoreLua`](../plugins/steam/steam-class-lua.cpp#L220) |
| `downloadScores` | [`onSteamDownloadScoresLua`](../plugins/steam/steam-class-lua.cpp#L259) |
| `cloudWrite` | [`onSteamCloudWriteLua`](../plugins/steam/steam-class-lua.cpp#L302) |
| `cloudRead` | [`onSteamCloudReadLua`](../plugins/steam/steam-class-lua.cpp#L314) |
| `cloudDelete` | [`onSteamCloudDeleteLua`](../plugins/steam/steam-class-lua.cpp#L339) |
| `cloudFileExists` | [`onSteamCloudFileExistsLua`](../plugins/steam/steam-class-lua.cpp#L349) |
| `isCloudEnabled` | [`onSteamIsCloudEnabledLua`](../plugins/steam/steam-class-lua.cpp#L359) |
| `activateOverlay` | [`onSteamActivateOverlayLua`](../plugins/steam/steam-class-lua.cpp#L375) |
| `activateOverlayURL` | [`onSteamActivateOverlayURLLua`](../plugins/steam/steam-class-lua.cpp#L384) |
| `showStore` | [`onSteamShowStoreLua`](../plugins/steam/steam-class-lua.cpp#L393) |
| `isDlcInstalled` | [`onSteamIsDlcInstalledLua`](../plugins/steam/steam-class-lua.cpp#L406) |
