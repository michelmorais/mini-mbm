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

#if PHYSICS_PARTICLES && !PHYSICS_UNIFIED
#include <Box2D/Box2D.h>
#else
#include <box2d/box2d.h>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr float step = 1.0f / 60.0f;

    void require(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    b2Body *box(b2World &world, float x, float y, float hx, float hy, bool dynamic)
    {
        b2BodyDef definition;
        definition.type = dynamic ? b2_dynamicBody : b2_staticBody;
        definition.position.Set(x, y);
        b2Body *body = world.CreateBody(&definition);
        b2PolygonShape shape;
        shape.SetAsBox(hx, hy);
        body->CreateFixture(&shape, dynamic ? 1.0f : 0.0f);
        return body;
    }

#if PHYSICS_PARTICLES
    struct PARTICLE_QUERY final : b2QueryCallback
    {
        int count = 0;
        bool ReportFixture(b2Fixture *) override { return true; }
        bool ReportParticle(const b2ParticleSystem *, int32) override
        {
            ++count;
            return true;
        }
    };

    struct PARTICLE_RAY final : b2RayCastCallback
    {
        int count = 0;
        float ReportFixture(b2Fixture *, const b2Vec2 &, const b2Vec2 &, float) override { return 1.0f; }
        float ReportParticle(const b2ParticleSystem *, int32, const b2Vec2 &, const b2Vec2 &, float) override
        {
            ++count;
            return 1.0f;
        }
    };

    struct PARTICLE_FILTER final : b2ContactFilter
    {
        bool ShouldCollide(b2Fixture *, b2ParticleSystem *, int32) override { return false; }
    };

    void particleCouplingCheck()
    {
        b2World world(b2Vec2(0, 0));
        b2Body *body = box(world, 0, 0, 0.1f, 1, true);
        b2ParticleSystemDef definition;
        definition.radius = 0.08f;
        b2ParticleSystem *system = world.CreateParticleSystem(&definition);
        b2PolygonShape shape;
        shape.SetAsBox(0.4f, 0.5f, b2Vec2(-1, 0), 0);
        b2ParticleGroupDef group;
        group.shape = &shape;
        group.linearVelocity.Set(5, 0);
        system->CreateParticleGroup(group);
        for (int i = 0; i < 30; ++i)
            world.Step(step, 8, 3);
        require(body->GetLinearVelocity().x > 0.1f, "particles did not push the dynamic body");
        float velocity = 0;
        const int count = system->GetParticleCount();
        for (int i = 0; i < count; ++i)
            velocity += system->GetVelocityBuffer()[i].x;
        require(velocity / count < 4.9f, "body did not slow down impacting particles");
    }

    void particleFilterCheck()
    {
        PARTICLE_FILTER filter;
        b2World world(b2Vec2(0, -10));
        world.SetContactFilter(&filter);
        box(world, 0, 0, 5, 0.2f, false);
        b2ParticleSystemDef definition;
        definition.radius = 0.08f;
        b2ParticleSystem *system = world.CreateParticleSystem(&definition);
        b2ParticleDef particle;
        particle.flags = b2_fixtureContactFilterParticle;
        particle.position.Set(0, 1);
        system->CreateParticle(particle);
        for (int i = 0; i < 90; ++i)
            world.Step(step, 8, 3);
        require(system->GetPositionBuffer()[0].y < -1, "fixture filter did not allow passage");
    }

    void particleFlagsCheck()
    {
        const uint32 flags[] = {b2_waterParticle, b2_viscousParticle, b2_elasticParticle,
            b2_springParticle, b2_powderParticle, b2_tensileParticle,
            b2_colorMixingParticle, b2_staticPressureParticle, b2_repulsiveParticle};
        for (uint32 flag : flags)
        {
            b2World world(b2Vec2(0, -10));
            // Tensile/repulsive particles spread beyond the initial group width.
            box(world, 0, -0.5f, 20, 0.5f, false);
            b2ParticleSystemDef definition;
            definition.radius = 0.08f;
            b2ParticleSystem *system = world.CreateParticleSystem(&definition);
            b2PolygonShape shape;
            shape.SetAsBox(0.4f, 0.5f, b2Vec2(0, 2), 0);
            b2ParticleGroupDef group;
            group.shape = &shape;
            group.flags = flag;
            system->CreateParticleGroup(group);
            const int count = system->GetParticleCount();
            for (int i = 0; i < 90; ++i)
                world.Step(step, 8, 3);
            require(count == system->GetParticleCount(), "flag scenario lost particles");
            for (int i = 0; i < count; ++i)
            {
                const b2Vec2 &position = system->GetPositionBuffer()[i];
                if (!std::isfinite(position.x) || !std::isfinite(position.y) || position.y <= -0.3f)
                    std::fprintf(stderr, "flag=%u particle=%d position=(%f,%f)\n", flag, i, position.x, position.y);
                require(std::isfinite(position.x) && std::isfinite(position.y) && position.y > -0.3f,
                        "flag scenario produced invalid or escaped particle");
            }
        }
    }

    void distanceQueriesCheck()
    {
        b2Transform transform;
        transform.SetIdentity();
        b2Vec2 normal;
        float distance = 0;
        b2CircleShape circle;
        circle.m_radius = 1;
        circle.ComputeDistance(transform, b2Vec2(0, 3), &distance, &normal, 0);
        require(std::abs(distance - 2) < 0.001f && normal.y > 0.99f, "circle distance/normal changed");
        b2PolygonShape polygon;
        polygon.SetAsBox(1, 1);
        polygon.ComputeDistance(transform, b2Vec2(0, 3), &distance, &normal, 0);
        require(std::abs(distance - 2) < 0.001f && normal.y > 0.99f, "polygon distance/normal changed");
        b2ChainShape chain;
        const b2Vec2 vertices[] = {b2Vec2(-1, 0), b2Vec2(1, 0)};
#if PHYSICS_UNIFIED
        chain.CreateChain(vertices, 2, b2Vec2(-2, 0), b2Vec2(2, 0));
#else
        chain.CreateChain(vertices, 2);
#endif
        chain.ComputeDistance(transform, b2Vec2(0, 3), &distance, &normal, 0);
        require(std::abs(distance - 3) < 0.001f && normal.y > 0.99f, "chain/edge distance changed");
    }
#endif

    void checks()
    {
        b2World world(b2Vec2(0.0f, -10.0f));
        box(world, 0, -0.5f, 20, 0.5f, false);
        b2Body *body = box(world, 0, 5, 0.5f, 0.5f, true);
        for (int i = 0; i < 300; ++i)
            world.Step(step, 8, 3);
        require(std::abs(body->GetPosition().y - 0.5f) < 0.05f, "falling box did not settle on floor");
        body->ApplyLinearImpulseToCenter(b2Vec2(2, 0), true);
        require(body->GetLinearVelocity().x > 1.9f, "linear impulse did not change velocity");
#if PHYSICS_PARTICLES
        b2ParticleSystemDef systemDefinition;
        systemDefinition.radius = 0.08f;
        b2ParticleSystem *system = world.CreateParticleSystem(&systemDefinition);
        box(world, -2, 2, 0.2f, 2, false);
        box(world, 2, 2, 0.2f, 2, false);
        b2PolygonShape shape;
        shape.SetAsBox(1.0f, 0.5f, b2Vec2(0, 2), 0);
        b2ParticleGroupDef groupDefinition;
        groupDefinition.shape = &shape;
        system->CreateParticleGroup(groupDefinition);
        const int initialCount = system->GetParticleCount();
        require(initialCount > 0, "no particles emitted");
        PARTICLE_QUERY query;
        b2AABB bounds;
        bounds.lowerBound.Set(-10, -10);
        bounds.upperBound.Set(10, 10);
        world.QueryAABB(&query, bounds);
        require(query.count == initialCount, "world query missed particles");
        PARTICLE_RAY ray;
        const float row = system->GetPositionBuffer()[0].y;
        world.RayCast(&ray, b2Vec2(-3, row), b2Vec2(3, row));
        require(ray.count > 0, "world ray cast missed particles");
        for (int i = 0; i < 180; ++i)
            world.Step(step, 8, 3);
        require(system->GetParticleCount() == initialCount, "unexpected particle loss");
        const b2Vec2 *positions = system->GetPositionBuffer();
        for (int i = 0; i < initialCount; ++i)
            require(std::isfinite(positions[i].x) && std::isfinite(positions[i].y) &&
                    positions[i].y > -0.2f && std::abs(positions[i].x) < 2.2f,
                    "particle escaped container or became nonfinite");
        for (int i = 0; i < initialCount; ++i)
            system->SetParticleLifetime(i, 0.1f);
        for (int i = 0; i < 30; ++i)
            world.Step(step, 8, 3);
        require(system->GetParticleCount() == 0, "particle lifetime did not expire");
        require(world.GetParticleSystemList() == system, "empty system unexpectedly destroyed");
        system->CreateParticleGroup(groupDefinition);
        require(system->GetParticleCount() == initialCount, "reemission after expiration changed count");
        world.DestroyParticleSystem(system);
        require(world.GetParticleSystemList() == nullptr, "particle system not removed");
        particleCouplingCheck();
        particleFilterCheck();
        particleFlagsCheck();
        distanceQueriesCheck();
#endif
        std::printf("PHYSICS_CHECK_OK %s\n", PHYSICS_BACKEND);
    }

    void benchmark(const char *scenario, int bodies, int systems, int particles, bool sleeping)
    {
        b2World world(b2Vec2(0, -10));
        world.SetAllowSleeping(sleeping);
        if (bodies > 0 || particles > 0)
        {
            box(world, 0, -0.5f, 30, 0.5f, false);
            box(world, -10, 5, 0.2f, 5, false);
            box(world, 10, 5, 0.2f, 5, false);
        }
        for (int i = 0; i < bodies; ++i)
            box(world, -8.0f + (i % 20) * 0.8f, 1.0f + (i / 20) * 0.8f, 0.3f, 0.3f, true);
#if PHYSICS_PARTICLES
        for (int i = 0; i < systems; ++i)
        {
            b2ParticleSystemDef definition;
            definition.radius = 0.05f;
            b2ParticleSystem *system = world.CreateParticleSystem(&definition);
            for (int j = 0; j < particles; ++j)
            {
                b2ParticleDef particle;
                particle.position.Set(-4 + (j % 100) * 0.08f, 1 + (j / 100) * 0.08f);
                system->CreateParticle(particle);
            }
        }
#else
        require(systems == 0 && particles == 0, "particles unavailable");
#endif
        for (int i = 0; i < 600; ++i)
            world.Step(step, 8, 3);
        std::vector<double> timings;
        timings.reserve(3600);
        for (int i = 0; i < 3600; ++i)
        {
            const auto start = std::chrono::steady_clock::now();
            world.Step(step, 8, 3);
            timings.push_back(std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - start).count());
        }
        double checksum = 0;
        for (b2Body *body = world.GetBodyList(); body; body = body->GetNext())
            checksum += body->GetPosition().x + body->GetPosition().y;
        require(std::isfinite(checksum), "nonfinite final body state");
        std::sort(timings.begin(), timings.end());
        std::printf("{\"backend\":\"%s\",\"scenario\":\"%s\",\"bodies\":%d,"
                    "\"systems\":%d,\"particles_per_system\":%d,\"samples\":3600,"
                    "\"median_us\":%.6f,\"p95_us\":%.6f,\"p99_us\":%.6f,\"body_checksum\":%.9f}\n",
                    PHYSICS_BACKEND, scenario, bodies, systems, particles,
                    timings[1800], timings[3420], timings[3564], checksum);
    }

    void rigidTrace()
    {
        b2World world(b2Vec2(0, -10));
        world.SetAllowSleeping(false);
        box(world, 0, -0.5f, 20, 0.5f, false);
        for (int i = 0; i < 20; ++i)
            box(world, (i % 5) * 0.9f, 1 + (i / 5) * 0.9f, 0.4f, 0.4f, true);
        b2Body *anchor = box(world, -3, 8, 0.1f, 0.1f, false);
        b2Body *pendulum = box(world, -1, 6, 0.3f, 0.3f, true);
        b2DistanceJointDef joint;
        joint.Initialize(anchor, pendulum, anchor->GetPosition(), pendulum->GetPosition());
        world.CreateJoint(&joint);
        std::uint64_t hash = 14695981039346656037ULL;
        for (int i = 0; i < 600; ++i)
        {
            world.Step(step, 8, 3);
            for (b2Body *body = world.GetBodyList(); body; body = body->GetNext())
            {
                const b2Vec2 &position = body->GetPosition();
                const b2Vec2 &velocity = body->GetLinearVelocity();
                const float values[] = {position.x, position.y, body->GetAngle(),
                                        velocity.x, velocity.y, body->GetAngularVelocity()};
                for (float value : values)
                {
                    require(std::isfinite(value), "nonfinite rigid trajectory");
                    std::uint32_t bits;
                    std::memcpy(&bits, &value, sizeof(bits));
                    hash = (hash ^ bits) * 1099511628211ULL;
                }
            }
        }
        std::printf("%016llx\n", static_cast<unsigned long long>(hash));
    }
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 2 || (std::strcmp(argv[1], "--check") && std::strcmp(argv[1], "--benchmark") && std::strcmp(argv[1], "--rigid-trace")))
            throw std::runtime_error("usage: physics_<backend> --check|--benchmark|--rigid-trace");
        if (!std::strcmp(argv[1], "--check"))
            checks();
        else if (!std::strcmp(argv[1], "--rigid-trace"))
            rigidTrace();
        else
        {
            benchmark("empty", 0, 0, 0, true);
            benchmark("rigid_sleep_allowed", 100, 0, 0, true);
            benchmark("rigid_awake", 100, 0, 0, false);
#if PHYSICS_PARTICLES
            benchmark("empty_system", 0, 1, 0, true);
            benchmark("empty_systems_100", 0, 100, 0, true);
            benchmark("empty_systems_1000", 0, 1000, 0, true);
            benchmark("particles_1000", 0, 1, 1000, true);
#endif
        }
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "PHYSICS_CHECK_FAILED: %s\n", error.what());
        return 1;
    }
    return 0;
}
