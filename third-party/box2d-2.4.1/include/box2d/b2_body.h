// MIT License

// Copyright (c) 2019 Erin Catto

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef B2_BODY_H
#define B2_BODY_H

#include "b2_api.h"
#include "b2_math.h"
#include "b2_shape.h"

class b2Fixture;
class b2Joint;
class b2Contact;
class b2Controller;
class b2World;
struct b2FixtureDef;
struct b2JointEdge;
struct b2ContactEdge;

/// The body type.
/// static: zero mass, zero velocity, may be manually moved
/// kinematic: zero mass, non-zero velocity set by user, moved by solver
/// dynamic: positive mass, non-zero velocity determined by forces, moved by solver
enum b2BodyType
{
	b2_staticBody = 0,
	b2_kinematicBody,
	b2_dynamicBody
};

/// A body definition holds all the data needed to construct a rigid body.
/// You can safely re-use body definitions. Shapes are added to a body after construction.
struct B2_API b2BodyDef
{
	/// This constructor sets the body definition default values.
	b2BodyDef()
	{
		position.Set(0.0f, 0.0f);
		angle = 0.0f;
		linearVelocity.Set(0.0f, 0.0f);
		angularVelocity = 0.0f;
		linearDamping = 0.0f;
		angularDamping = 0.0f;
		allowSleep = true;
		awake = true;
		fixedRotation = false;
		bullet = false;
		type = b2_staticBody;
		enabled = true;
		gravityScale = 1.0f;
	}

	/// The body type: static, kinematic, or dynamic.
	/// Note: if a dynamic body would have zero mass, the mass is set to one.
	b2BodyType type;

	/// The world position of the body. Avoid creating bodies at the origin
	/// since this can lead to many overlapping shapes.
	b2Vec2 position;

	/// The world angle of the body in radians.
	float angle;

	/// The linear velocity of the body's origin in world co-ordinates.
	b2Vec2 linearVelocity;

	/// The angular velocity of the body.
	float angularVelocity;

	/// Linear damping is use to reduce the linear velocity. The damping parameter
	/// can be larger than 1.0f but the damping effect becomes sensitive to the
	/// time step when the damping parameter is large.
	/// Units are 1/time
	float linearDamping;

	/// Angular damping is use to reduce the angular velocity. The damping parameter
	/// can be larger than 1.0f but the damping effect becomes sensitive to the
	/// time step when the damping parameter is large.
	/// Units are 1/time
	float angularDamping;

	/// Set this flag to false if this body should never fall asleep. Note that
	/// this increases CPU usage.
	bool allowSleep;

	/// Is this body initially awake or sleeping?
	bool awake;

	/// Should this body be prevented from rotating? Useful for characters.
	bool fixedRotation;

	/// Is this a fast moving body that should be prevented from tunneling through
	/// other moving bodies? Note that all bodies are prevented from tunneling through
	/// kinematic and static bodies. This setting is only considered on dynamic bodies.
	/// @warning You should use this flag sparingly since it increases processing time.
	bool bullet;

	/// Does this body start out enabled?
	bool enabled;

	/// Use this to store application specific body data.
	void * userData;

	/// Scale the gravity applied to this body.
	float gravityScale;
};

/// A rigid body. These are created via b2World::CreateBody.
class B2_API b2Body
{
public:
	/// Creates a fixture and attach it to this body. Use this function if you need
	/// to set some fixture parameters, like friction. Otherwise you can create the
	/// fixture directly from a shape.
	/// If the density is non-zero, this function automatically updates the mass of the body.
	/// Contacts are not created until the next time step.
	/// @param def the fixture definition.
	/// @warning This function is locked during callbacks.
	b2Fixture* CreateFixture(const b2FixtureDef* def);

	/// Creates a fixture from a shape and attach it to this body.
	/// This is a convenience function. Use b2FixtureDef if you need to set parameters
	/// like friction, restitution, user data, or filtering.
	/// If the density is non-zero, this function automatically updates the mass of the body.
	/// @param shape the shape to be cloned.
	/// @param density the shape density (set to zero for static bodies).
	/// @warning This function is locked during callbacks.
	b2Fixture* CreateFixture(const b2Shape* shape, float density);

	/// Destroy a fixture. This removes the fixture from the broad-phase and
	/// destroys all contacts associated with this fixture. This will
	/// automatically adjust the mass of the body if the body is dynamic and the
	/// fixture has positive density.
	/// All fixtures attached to a body are implicitly destroyed when the body is destroyed.
	/// @param fixture the fixture to be removed.
	/// @warning This function is locked during callbacks.
	void DestroyFixture(b2Fixture* fixture);

	/// Set the position of the body's origin and rotation.
	/// Manipulating a body's transform may cause non-physical behavior.
	/// Note: contacts are updated on the next call to b2World::Step.
	/// @param position the world position of the body's local origin.
	/// @param angle the world rotation in radians.
	void SetTransform(const b2Vec2& position, float angle);

	/// Get the body transform for the body's origin.
	/// @return the world transform of the body's origin.
	const b2Transform& GetTransform() const;

	/// Get the world body origin position.
	/// @return the world position of the body's origin.
	const b2Vec2& GetPosition() const;

	/// Get the angle in radians.
	/// @return the current world rotation angle in radians.
	float GetAngle() const;

	/// Get the world position of the center of mass.
	const b2Vec2& GetWorldCenter() const;

	/// Get the local position of the center of mass.
	const b2Vec2& GetLocalCenter() const;

	/// Set the linear velocity of the center of mass.
	/// @param v the new linear velocity of the center of mass.
	void SetLinearVelocity(const b2Vec2& v);

	/// Get the linear velocity of the center of mass.
	/// @return the linear velocity of the center of mass.
	const b2Vec2& GetLinearVelocity() const;

	/// Set the angular velocity.
	/// @param omega the new angular velocity in radians/second.
	void SetAngularVelocity(float omega);

	/// Get the angular velocity.
	/// @return the angular velocity in radians/second.
	float GetAngularVelocity() const;

	/// Apply a force at a world point. If the force is not
	/// applied at the center of mass, it will generate a torque and
	/// affect the angular velocity. This wakes up the body.
	/// @param force the world force vector, usually in Newtons (N).
	/// @param point the world position of the point of application.
	/// @param wake also wake up the body
	void ApplyForce(const b2Vec2& force, const b2Vec2& point, bool wake);

	/// Apply a force to the center of mass. This wakes up the body.
	/// @param force the world force vector, usually in Newtons (N).
	/// @param wake also wake up the body
	void ApplyForceToCenter(const b2Vec2& force, bool wake);

	/// Apply a torque. This affects the angular velocity
	/// without affecting the linear velocity of the center of mass.
	/// @param torque about the z-axis (out of the screen), usually in N-m.
	/// @param wake also wake up the body
	void ApplyTorque(float torque, bool wake);

	/// Apply an impulse at a point. This immediately modifies the velocity.
	/// It also modifies the angular velocity if the point of application
	/// is not at the center of mass. This wakes up the body.
	/// @param impulse the world impulse vector, usually in N-seconds or kg-m/s.
	/// @param point the world position of the point of application.
	/// @param wake also wake up the body
	void ApplyLinearImpulse(const b2Vec2& impulse, const b2Vec2& point, bool wake);

	/// Apply an impulse to the center of mass. This immediately modifies the velocity.
	/// @param impulse the world impulse vector, usually in N-seconds or kg-m/s.
	/// @param wake also wake up the body
	void ApplyLinearImpulseToCenter(const b2Vec2& impulse, bool wake);

	/// Apply an angular impulse.
	/// @param impulse the angular impulse in units of kg*m*m/s
	/// @param wake also wake up the body
	void ApplyAngularImpulse(float impulse, bool wake);

	/// Get the total mass of the body.
	/// @return the mass, usually in kilograms (kg).
	float GetMass() const;

	/// Get the rotational inertia of the body about the local origin.
	/// @return the rotational inertia, usually in kg-m^2.
	float GetInertia() const;

	/// Get the mass data of the body.
	/// @return a struct containing the mass, inertia and center of the body.
	b2MassData GetMassData() const;

	/// Set the mass properties to override the mass properties of the fixtures.
	/// Note that this changes the center of mass position.
	/// Note that creating or destroying fixtures can also alter the mass.
	/// This function has no effect if the body isn't dynamic.
	/// @param data the mass properties.
	void SetMassData(const b2MassData* data);

	/// This resets the mass properties to the sum of the mass properties of the fixtures.
	/// This normally does not need to be called unless you called SetMassData to override
	/// the mass and you later want to reset the mass.
	void ResetMassData();

	/// Get the world coordinates of a point given the local coordinates.
	/// @param localPoint a point on the body measured relative the the body's origin.
	/// @return the same point expressed in world coordinates.
	b2Vec2 GetWorldPoint(const b2Vec2& localPoint) const;

	/// Get the world coordinates of a vector given the local coordinates.
	/// @param localVector a vector fixed in the body.
	/// @return the same vector expressed in world coordinates.
	b2Vec2 GetWorldVector(const b2Vec2& localVector) const;

	/// Gets a local point relative to the body's origin given a world point.
	/// @param worldPoint a point in world coordinates.
	/// @return the corresponding local point relative to the body's origin.
	b2Vec2 GetLocalPoint(const b2Vec2& worldPoint) const;

	/// Gets a local vector given a world vector.
	/// @param worldVector a vector in world coordinates.
	/// @return the corresponding local vector.
	b2Vec2 GetLocalVector(const b2Vec2& worldVector) const;

	/// Get the world linear velocity of a world point attached to this body.
	/// @param worldPoint a point in world coordinates.
	/// @return the world velocity of a point.
	b2Vec2 GetLinearVelocityFromWorldPoint(const b2Vec2& worldPoint) const;

	/// Get the world velocity of a local point.
	/// @param localPoint a point in local coordinates.
	/// @return the world velocity of a point.
	b2Vec2 GetLinearVelocityFromLocalPoint(const b2Vec2& localPoint) const;

	/// Get the linear damping of the body.
	float GetLinearDamping() const;

	/// Set the linear damping of the body.
	void SetLinearDamping(float linearDamping);

	/// Get the angular damping of the body.
	float GetAngularDamping() const;

	/// Set the angular damping of the body.
	void SetAngularDamping(float angularDamping);

	/// Get the gravity scale of the body.
	float GetGravityScale() const;

	/// Set the gravity scale of the body.
	void SetGravityScale(float scale);

	/// Set the type of this body. This may alter the mass and velocity.
	void SetType(b2BodyType type);

	/// Get the type of this body.
	b2BodyType GetType() const;

	/// Should this body be treated like a bullet for continuous collision detection?
	void SetBullet(bool flag);

	/// Is this body treated like a bullet for continuous collision detection?
	bool IsBullet() const;

	/// You can disable sleeping on this body. If you disable sleeping, the
	/// body will be woken.
	void SetSleepingAllowed(bool flag);

	/// Is this body allowed to sleep
	bool IsSleepingAllowed() const;

	/// Set the sleep state of the body. A sleeping body has very
	/// low CPU cost.
	/// @param flag set to true to wake the body, false to put it to sleep.
	void SetAwake(bool flag);

	/// Get the sleeping state of this body.
	/// @return true if the body is awake.
	bool IsAwake() const;

	/// Allow a body to be disabled. A disabled body is not simulated and cannot
	/// be collided with or woken up.
	/// If you pass a flag of true, all fixtures will be added to the broad-phase.
	/// If you pass a flag of false, all fixtures will be removed from the
	/// broad-phase and all contacts will be destroyed.
	/// Fixtures and joints are otherwise unaffected. You may continue
	/// to create/destroy fixtures and joints on disabled bodies.
	/// Fixtures on a disabled body are implicitly disabled and will
	/// not participate in collisions, ray-casts, or queries.
	/// Joints connected to a disabled body are implicitly disabled.
	/// An diabled body is still owned by a b2World object and remains
	/// in the body list.
	void SetEnabled(bool flag);

	/// Get the active state of the body.
	bool IsEnabled() const;

	/// Set this body to have fixed rotation. This causes the mass
	/// to be reset.
	void SetFixedRotation(bool flag);

	/// Does this body have fixed rotation?
	bool IsFixedRotation() const;

	/// Get the list of all fixtures attached to this body.
	b2Fixture* GetFixtureList();
	const b2Fixture* GetFixtureList() const;

	/// Get the list of all joints attached to this body.
	b2JointEdge* GetJointList();
	const b2JointEdge* GetJointList() const;

	/// Get the list of all contacts attached to this body.
	/// @warning this list changes during the time step and you may
	/// miss some collisions if you don't use b2ContactListener.
	b2ContactEdge* GetContactList();
	const b2ContactEdge* GetContactList() const;

	/// Get the next body in the world's body list.
	b2Body* GetNext();
	const b2Body* GetNext() const;

	/// Get the user data pointer that was provided in the body definition.
	void* GetUserData();

	/// Set the user data. Use this to store your application specific data.
	void SetUserData(void* data);

	/// Get the parent world of this body.
	b2World* GetWorld();
	const b2World* GetWorld() const;

	/// Dump this body to a file
	void Dump();

private:

	friend class b2World;
	friend class b2Island;
	friend class b2ContactManager;
	friend class b2ContactSolver;
	friend class b2Contact;

	friend class b2DistanceJoint;
	friend class b2FrictionJoint;
	friend class b2GearJoint;
	friend class b2MotorJoint;
	friend class b2MouseJoint;
	friend class b2PrismaticJoint;
	friend class b2PulleyJoint;
	friend class b2RevoluteJoint;
	friend class b2WeldJoint;
	friend class b2WheelJoint;

	// m_flags
	enum
	{
		e_islandFlag		= 0x0001,
		e_awakeFlag			= 0x0002,
		e_autoSleepFlag		= 0x0004,
		e_bulletFlag		= 0x0008,
		e_fixedRotationFlag	= 0x0010,
		e_enabledFlag		= 0x0020,
		e_toiFlag			= 0x0040
	};

	b2Body(const b2BodyDef* bd, b2World* world);
	~b2Body();

	void SynchronizeFixtures();
	void SynchronizeTransform();

	// This is used to prevent connected bodies from colliding.
	// It may lie, depending on the collideConnected flag.
	bool ShouldCollide(const b2Body* other) const;

	void Advance(float t);

	b2BodyType m_type;

	uint16 m_flags;

	int32 m_islandIndex;

	b2Transform m_xf;		// the body origin transform
	b2Sweep m_sweep;		// the swept motion for CCD

	b2Vec2 m_linearVelocity;
	float m_angularVelocity;

	b2Vec2 m_force;
	float m_torque;

	b2World* m_world;
	b2Body* m_prev;
	b2Body* m_next;

	b2Fixture* m_fixtureList;
	int32 m_fixtureCount;

	b2JointEdge* m_jointList;
	b2ContactEdge* m_contactList;

	float m_mass, m_invMass;

	// Rotational inertia about the center of mass.
	float m_I, m_invI;

	float m_linearDamping;
	float m_angularDamping;
	float m_gravityScale;

	float m_sleepTime;

	void* m_userData;
};

inline b2BodyType b2Body::GetType() const
{
	return m_type;
}

inline const b2Transform& b2Body::GetTransform() const
{
	return m_xf;
}

inline const b2Vec2& b2Body::GetPosition() const
{
	return m_xf.p;
}

inline float b2Body::GetAngle() const
{
	return m_sweep.a;
}

inline const b2Vec2& b2Body::GetWorldCenter() const
{
	return m_sweep.c;
}

inline const b2Vec2& b2Body::GetLocalCenter() const
{
	return m_sweep.localCenter;
}

inline void b2Body::SetLinearVelocity(const b2Vec2& v)
{
	if (m_type == b2_staticBody)
	{
		return;
	}

	if (b2Dot(v,v) > 0.0f)
	{
		SetAwake(true);
	}

	m_linearVelocity = v;
}

inline const b2Vec2& b2Body::GetLinearVelocity() const
{
	return m_linearVelocity;
}

inline void b2Body::SetAngularVelocity(float w)
{
	if (m_type == b2_staticBody)
	{
		return;
	}

	if (w * w > 0.0f)
	{
		SetAwake(true);
	}

	m_angularVelocity = w;
}

inline float b2Body::GetAngularVelocity() const
{
	return m_angularVelocity;
}

inline float b2Body::GetMass() const
{
	return m_mass;
}

inline float b2Body::GetInertia() const
{
	return m_I + m_mass * b2Dot(m_sweep.localCenter, m_sweep.localCenter);
}

inline b2MassData b2Body::GetMassData() const
{
	b2MassData data;
	data.mass = m_mass;
	data.I = m_I + m_mass * b2Dot(m_sweep.localCenter, m_sweep.localCenter);
	data.center = m_sweep.localCenter;
	return data;
}

inline b2Vec2 b2Body::GetWorldPoint(const b2Vec2& localPoint) const
{
	return b2Mul(m_xf, localPoint);
}

inline b2Vec2 b2Body::GetWorldVector(const b2Vec2& localVector) const
{
	return b2Mul(m_xf.q, localVector);
}

inline b2Vec2 b2Body::GetLocalPoint(const b2Vec2& worldPoint) const
{
	return b2MulT(m_xf, worldPoint);
}

inline b2Vec2 b2Body::GetLocalVector(const b2Vec2& worldVector) const
{
	return b2MulT(m_xf.q, worldVector);
}

inline b2Vec2 b2Body::GetLinearVelocityFromWorldPoint(const b2Vec2& worldPoint) const
{
	return m_linearVelocity + b2Cross(m_angularVelocity, worldPoint - m_sweep.c);
}

inline b2Vec2 b2Body::GetLinearVelocityFromLocalPoint(const b2Vec2& localPoint) const
{
	return GetLinearVelocityFromWorldPoint(GetWorldPoint(localPoint));
}

inline float b2Body::GetLinearDamping() const
{
	return m_linearDamping;
}

inline void b2Body::SetLinearDamping(float linearDamping)
{
	m_linearDamping = linearDamping;
}

inline float b2Body::GetAngularDamping() const
{
	return m_angularDamping;
}

inline void b2Body::SetAngularDamping(float angularDamping)
{
	m_angularDamping = angularDamping;
}

inline float b2Body::GetGravityScale() const
{
	return m_gravityScale;
}

inline void b2Body::SetGravityScale(float scale)
{
	m_gravityScale = scale;
}

inline void b2Body::SetBullet(bool flag)
{
	if (flag)
	{
		m_flags |= e_bulletFlag;
	}
	else
	{
		m_flags &= ~e_bulletFlag;
	}
}

inline bool b2Body::IsBullet() const
{
	return (m_flags & e_bulletFlag) == e_bulletFlag;
}

inline void b2Body::SetAwake(bool flag)
{
	if (m_type == b2_staticBody)
	{
		return;
	}

	if (flag)
	{
		m_flags |= e_awakeFlag;
		m_sleepTime = 0.0f;
	}
	else
	{
		m_flags &= ~e_awakeFlag;
		m_sleepTime = 0.0f;
		m_linearVelocity.SetZero();
		m_angularVelocity = 0.0f;
		m_force.SetZero();
		m_torque = 0.0f;
	}
}

inline bool b2Body::IsAwake() const
{
	return (m_flags & e_awakeFlag) == e_awakeFlag;
}

inline bool b2Body::IsEnabled() const
{
	return (m_flags & e_enabledFlag) == e_enabledFlag;
}

inline bool b2Body::IsFixedRotation() const
{
	return (m_flags & e_fixedRotationFlag) == e_fixedRotationFlag;
}

inline void b2Body::SetSleepingAllowed(bool flag)
{
	if (flag)
	{
		m_flags |= e_autoSleepFlag;
	}
	else
	{
		m_flags &= ~e_autoSleepFlag;
		SetAwake(true);
	}
}

inline bool b2Body::IsSleepingAllowed() const
{
	return (m_flags & e_autoSleepFlag) == e_autoSleepFlag;
}

inline b2Fixture* b2Body::GetFixtureList()
{
	return m_fixtureList;
}

inline const b2Fixture* b2Body::GetFixtureList() const
{
	return m_fixtureList;
}

inline b2JointEdge* b2Body::GetJointList()
{
	return m_jointList;
}

inline const b2JointEdge* b2Body::GetJointList() const
{
	return m_jointList;
}

inline b2ContactEdge* b2Body::GetContactList()
{
	return m_contactList;
}

inline const b2ContactEdge* b2Body::GetContactList() const
{
	return m_contactList;
}

inline b2Body* b2Body::GetNext()
{
	return m_next;
}

inline const b2Body* b2Body::GetNext() const
{
	return m_next;
}

inline void* b2Body::GetUserData()
{
	return m_userData;
}

inline void b2Body::SetUserData(void * data)
{
	m_userData = data;
}

inline void b2Body::ApplyForce(const b2Vec2& force, const b2Vec2& point, bool wake)
{
	if (m_type != b2_dynamicBody)
	{
		return;
	}

	if (wake && (m_flags & e_awakeFlag) == 0)
	{
		SetAwake(true);
	}

	// Don't accumulate a force if the body is sleeping.
	if (m_flags & e_awakeFlag)
	{
		m_force += force;
		m_torque += b2Cross(point - m_sweep.c, force);
	}
}

inline void b2Body::ApplyForceToCenter(const b2Vec2& force, bool wake)
{
	if (m_type != b2_dynamicBody)
	{
		return;
	}

	if (wake && (m_flags & e_awakeFlag) == 0)
	{
		SetAwake(true);
	}

	// Don't accumulate a force if the body is sleeping
	if (m_flags & e_awakeFlag)
	{
		m_force += force;
	}
}

inline void b2Body::ApplyTorque(float torque, bool wake)
{
	if (m_type != b2_dynamicBody)
	{
		return;
	}

	if (wake && (m_flags & e_awakeFlag) == 0)
	{
		SetAwake(true);
	}

	// Don't accumulate a force if the body is sleeping
	if (m_flags & e_awakeFlag)
	{
		m_torque += torque;
	}
}

inline void b2Body::ApplyLinearImpulse(const b2Vec2& impulse, const b2Vec2& point, bool wake)
{
	if (m_type != b2_dynamicBody)
	{
		return;
	}

	if (wake && (m_flags & e_awakeFlag) == 0)
	{
		SetAwake(true);
	}

	// Don't accumulate velocity if the body is sleeping
	if (m_flags & e_awakeFlag)
	{
		m_linearVelocity += m_invMass * impulse;
		m_angularVelocity += m_invI * b2Cross(point - m_sweep.c, impulse);
	}
}

inline void b2Body::ApplyLinearImpulseToCenter(const b2Vec2& impulse, bool wake)
{
	if (m_type != b2_dynamicBody)
	{
		return;
	}

	if (wake && (m_flags & e_awakeFlag) == 0)
	{
		SetAwake(true);
	}

	// Don't accumulate velocity if the body is sleeping
	if (m_flags & e_awakeFlag)
	{
		m_linearVelocity += m_invMass * impulse;
	}
}

inline void b2Body::ApplyAngularImpulse(float impulse, bool wake)
{
	if (m_type != b2_dynamicBody)
	{
		return;
	}

	if (wake && (m_flags & e_awakeFlag) == 0)
	{
		SetAwake(true);
	}

	// Don't accumulate velocity if the body is sleeping
	if (m_flags & e_awakeFlag)
	{
		m_angularVelocity += m_invI * impulse;
	}
}

inline void b2Body::SynchronizeTransform()
{
	m_xf.q.Set(m_sweep.a);
	m_xf.p = m_sweep.c - b2Mul(m_xf.q, m_sweep.localCenter);
}

inline void b2Body::Advance(float alpha)
{
	// Advance to the new safe time. This doesn't sync the broad-phase.
	m_sweep.Advance(alpha);
	m_sweep.c = m_sweep.c0;
	m_sweep.a = m_sweep.a0;
	m_xf.q.Set(m_sweep.a);
	m_xf.p = m_sweep.c - b2Mul(m_xf.q, m_sweep.localCenter);
}

inline b2World* b2Body::GetWorld()
{
	return m_world;
}

inline const b2World* b2Body::GetWorld() const
{
	return m_world;
}

#endif
