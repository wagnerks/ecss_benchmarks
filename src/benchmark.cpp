#include <benchmark/benchmark.h>
#include <entt/entt.hpp>
#include <flecs.h>
#include <ecss/Registry.h>

#include <vector>
#include <cstdint>

struct Position { float x, y, z; };
struct Velocity { float vx, vy, vz; };

// Combined entity for vector baseline (AoS layout)
struct Entity {
    uint32_t id;
    Position pos;
    Velocity vel;
    bool hasPos = false;
    bool hasVel = false;
};

// ================= std::vector baseline benchmarks =====================
namespace vec {
    // Insert Position component (push_back to vector)
    static void insert(benchmark::State& state) {
        for (auto _ : state) {
            std::vector<Position> positions;
            positions.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                positions.push_back(Position{42.f, 42.f, 42.f});
            }
            benchmark::DoNotOptimize(positions.data());
        }
    }

    // Create entities only (reserve + resize, no components)
    static void create_entities(benchmark::State& state) {
        for (auto _ : state) {
            std::vector<uint32_t> ids;
            ids.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                ids.push_back(static_cast<uint32_t>(i));
            }
            benchmark::DoNotOptimize(ids.data());
        }
    }

    // Add Position component with varying values
    static void add_int_component(benchmark::State& state) {
        for (auto _ : state) {
            std::vector<Position> positions;
            positions.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                positions.push_back(Position{(float)i, (float)i + 1.f, (float)i + 2.f});
            }
            benchmark::DoNotOptimize(positions.data());
        }
    }

    // Add Velocity component
    static void add_struct_component(benchmark::State& state) {
        for (auto _ : state) {
            std::vector<Velocity> velocities;
            velocities.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                velocities.push_back(Velocity{(float)i, (float)i * 2.f, (float)i * 3.f});
            }
            benchmark::DoNotOptimize(velocities.data());
        }
    }

    // Insert two components (Entity with Position + Velocity)
    static void grouped_insert(benchmark::State& state) {
        for (auto _ : state) {
            std::vector<Entity> entities;
            entities.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                entities.push_back(Entity{
                    static_cast<uint32_t>(i),
                    Position{7.f, 8.f, 9.f},
                    Velocity{1.f, 2.f, 3.f},
                    true, true
                });
            }
            benchmark::DoNotOptimize(entities.data());
        }
    }

    // hasComponent equivalent (check bool flag)
    static void has_component(benchmark::State& state) {
        std::vector<Entity> entities;
        entities.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            entities.push_back(Entity{
                static_cast<uint32_t>(i),
                Position{1.f, 2.f, 3.f},
                Velocity{0.f, 0.f, 0.f},
                true, false
            });
        }
        for (auto _ : state) {
            size_t count = 0;
            for (const auto& e : entities) {
                if (e.hasPos) ++count;
            }
            benchmark::DoNotOptimize(count);
        }
    }

    // Destroy entities - mark as deleted (similar to ECS soft-delete)
    // Note: vector clear() is O(1) which isn't comparable to ECS destroy
    // So we simulate per-entity deletion by zeroing (touching each element)
    static void destroy_entities(benchmark::State& state) {
        std::vector<Entity> entities;
        entities.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            entities.push_back(Entity{
                static_cast<uint32_t>(i),
                Position{0.f, 0.f, 0.f},
                Velocity{0.f, 0.f, 0.f},
                true, true
            });
        }
        for (auto _ : state) {
            // Simulate destroy by marking each entity as deleted
            for (auto& e : entities) {
                e.hasPos = false;
                e.hasVel = false;
            }
            benchmark::ClobberMemory();
            // Reset for next iteration
            state.PauseTiming();
            for (auto& e : entities) {
                e.hasPos = true;
                e.hasVel = true;
            }
            state.ResumeTiming();
        }
    }

    // Iterate single component (vector<Position>)
    static void iter_single_component(benchmark::State& state) {
        std::vector<Position> positions;
        positions.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            positions.push_back(Position{(float)i, (float)i + 1.f, (float)i + 2.f});
        }
        for (auto _ : state) {
            float sum = 0.f;
            for (const auto& p : positions) {
                sum += p.x + p.y + p.z;
            }
            benchmark::DoNotOptimize(sum);
        }
    }

    // Iterate multi component (Entity with Position + Velocity - AoS)
    static void iter_grouped_multi(benchmark::State& state) {
        std::vector<Entity> entities;
        entities.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            entities.push_back(Entity{
                static_cast<uint32_t>(i),
                Position{(float)i, (float)i * 2.f, (float)i * 3.f},
                Velocity{(float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f},
                true, true
            });
        }
        for (auto _ : state) {
            float accum = 0.f;
            for (const auto& e : entities) {
                accum += e.pos.x + e.pos.y + e.pos.z + e.vel.vx + e.vel.vy + e.vel.vz;
            }
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate separate (same as grouped for vector - no difference)
    static void iter_separate_multi(benchmark::State& state) {
        iter_grouped_multi(state);
    }

    // Iterate sparse intersection - N entities with A, N with B, but only 1/50 have both
    static void iter_sparse_multi(benchmark::State& state) {
        const int n = state.range(0);
        const int intersection = n / 50; // 2% intersection
        std::vector<Entity> entities;
        entities.reserve(intersection);
        for (int i = 0; i < intersection; ++i) {
            entities.push_back(Entity{
                static_cast<uint32_t>(i),
                Position{(float)i, (float)i * 2.f, (float)i * 3.f},
                Velocity{(float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f},
                true, true
            });
        }
        for (auto _ : state) {
            float accum = 0.f;
            for (const auto& e : entities) {
                accum += e.pos.x + e.pos.y + e.pos.z + e.vel.vx + e.vel.vy + e.vel.vz;
            }
            benchmark::DoNotOptimize(accum);
        }
    }
}

namespace ecss
{
    // Insert Position component per entity (constant values)
    static void insert(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<Position>(e, Position{ 42.f, 42.f, 42.f });
            }
        }
    }

    // Create entities only (no components)
    static void create_entities(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            for (int i = 0; i < state.range(0); ++i) {
                benchmark::DoNotOptimize(reg.takeEntity());
            }
        }
    }

    // Add Position component with varying values
    static void add_int_component(benchmark::State& state) { // name kept for macro compatibility
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
            }
        }
    }

    // Add Velocity component (acts as former struct benchmark distinction)
    static void add_struct_component(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<Velocity>(e, Velocity{ (float)i, (float)i * 2.f, (float)i * 3.f });
            }
        }
    }

    // Grouped array registration then insert two components (Position + Velocity)
    static void grouped_insert(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            reg.registerArray<Position, Velocity>();
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<Position>(e, Position{ 7.f, 8.f, 9.f });
                reg.addComponent<Velocity>(e, Velocity{ 1.f, 2.f, 3.f });
            }
        }
    }

    // hasComponent over existing components (Position)
    static void has_component(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        using ecss::EntityId;
        Reg reg;
        std::vector<EntityId> ids;
        ids.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{1.f,2.f,3.f});
            ids.push_back(e);
        }
        for (auto _ : state) {
            size_t count = 0;
            for (auto id : ids) {
                if (reg.hasComponent<Position>(id)) ++count;
            }
            benchmark::DoNotOptimize(count);
        }
    }

    // Batch destroy entities (Position + Velocity)
    static void destroy_entities(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        using ecss::EntityId;
        for (auto _ : state) {
            state.PauseTiming();
            Reg reg;
            std::vector<EntityId> ids; ids.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<Position>(e, Position{ 0.f,0.f,0.f });
                reg.addComponent<Velocity>(e, Velocity{ 0.f,0.f,0.f });
                ids.push_back(e);
            }
            state.ResumeTiming();
            reg.destroyEntities(ids);
            benchmark::ClobberMemory();
        }
    }

    // ================= Iteration Benchmarks =====================
    // Iterate a single Position component array
    static void iter_single_component(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        Reg reg;
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
        }
        auto view = reg.view<Position>();
        for (auto _ : state) {
            float sum = 0.f;
            for (auto [eid, p] : view) {
                sum += p->x + p->y + p->z;
            }
            benchmark::DoNotOptimize(sum);
        }
    }

    // Iterate multiple components that are grouped (Position + Velocity)
    static void iter_grouped_multi(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        Reg reg;
        reg.registerArray<Position, Velocity>();
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{ (float)i, (float)i * 2.f, (float)i * 3.f });
            reg.addComponent<Velocity>(e, Velocity{ (float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f });
        }
        auto view = reg.view<Position, Velocity>();
        for (auto _ : state) {
            float accum = 0.f;
            for (auto [eid, pptr, vptr] : view) {
                accum += pptr->x + pptr->y + pptr->z + vptr->vx + vptr->vy + vptr->vz;
            }
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate multiple components in separate arrays (no grouping call)
    static void iter_separate_multi(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        Reg reg; // no grouping
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
            reg.addComponent<Velocity>(e, Velocity{ (float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f });
        }
        auto view = reg.view<Position, Velocity>();
        for (auto _ : state) {
            float accum = 0.f;
            for (auto [eid, pptr, vptr] : view) {
                accum += pptr->x + pptr->y + pptr->z + vptr->vx + vptr->vy + vptr->vz;
            }
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate sparse intersection - N entities with Position, N/50 with Velocity
    // Use Velocity as primary (smaller set) for efficient iteration
    static void iter_sparse_multi(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        Reg reg;
        const int n = state.range(0);
        const int step = 50; // every 50th entity has both components
        
        // Create N entities with Position
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
        }
        // Add Velocity only to every 50th entity (2% intersection)
        for (int i = 0; i < n; i += step) {
            reg.addComponent<Velocity>(ecss::EntityId(i), Velocity{ (float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f });
        }
        
        // Velocity first = iterate smaller set (n/50), lookup Position
        auto view = reg.view<Velocity, Position>();
        for (auto _ : state) {
            float accum = 0.f;
            for (auto [eid, vptr, pptr] : view) {
                if (pptr && vptr) {
                    accum += pptr->x + pptr->y + pptr->z + vptr->vx + vptr->vy + vptr->vz;
                }
            }
            benchmark::DoNotOptimize(accum);
        }
    }
}

namespace entt
{
    using big_registry = entt::registry;
    // Insert Position component per entity (constant)
    static void insert(benchmark::State& state) {
        for (auto _ : state) {
            big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<Position>(e, Position{42.f,42.f,42.f});
            }
        }
    }

    // Create entities only
    static void create_entities(benchmark::State& state) {
        for (auto _ : state) {
            big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                benchmark::DoNotOptimize(reg.create());
            }
        }
    }

    // Add Position component varying values
    static void add_int_component(benchmark::State& state) { // name kept
        for (auto _ : state) {
            big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
            }
        }
    }

    // Add Velocity component
    static void add_struct_component(benchmark::State& state) {
        for (auto _ : state) {
            big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<Velocity>(e, Velocity{ (float)i, (float)i * 2.f, (float)i * 3.f });
            }
        }
    }

    // Multi-component insert (Position + Velocity)
    static void grouped_insert(benchmark::State& state) {
        for (auto _ : state) {
            big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<Position>(e, Position{ 7.f,8.f,9.f });
                reg.emplace<Velocity>(e, Velocity{ 1.f,2.f,3.f });
            }
        }
    }

    // hasComponent equivalent for Position
    static void has_component(benchmark::State& state) {
        big_registry reg;
        std::vector<big_registry::entity_type> ids; ids.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, Position{1.f,2.f,3.f});
            ids.push_back(e);
        }
        for (auto _ : state) {
            size_t count = 0;
            for (auto e : ids) {
                if (reg.all_of<Position>(e)) ++count;
            }
            benchmark::DoNotOptimize(count);
        }
    }

    // Batch destroy (Position + Velocity)
    static void destroy_entities(benchmark::State& state) {
        for (auto _ : state) {
            state.PauseTiming();
            big_registry reg;
            std::vector<big_registry::entity_type> ids; ids.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<Position>(e, Position{0.f,0.f,0.f});
                reg.emplace<Velocity>(e, Velocity{0.f,0.f,0.f});
                ids.push_back(e);
            }
            state.ResumeTiming();
            reg.destroy(ids.begin(), ids.end());
            benchmark::ClobberMemory();
        }
    }

    // Iteration single component (Position)
    static void iter_single_component(benchmark::State& state) {
        big_registry reg;
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
        }
        auto view = reg.view<Position>();
        for (auto _ : state) {
            float sum = 0.f;
            for (auto entity : view) {
                auto &val = view.get<Position>(entity);
                sum += val.x + val.y + val.z;
            }
            benchmark::DoNotOptimize(sum);
        }
    }

    // Iteration multi component (Position + Velocity)
    static void iter_grouped_multi(benchmark::State& state) {
        big_registry reg;
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, Position{ (float)i, (float)i * 2.f, (float)i * 3.f });
            reg.emplace<Velocity>(e, Velocity{ (float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f });
        }
        auto view = reg.view<Position, Velocity>();
        for (auto _ : state) {
            float accum = 0.f;
            for (auto entity : view) {
                auto [pos, vel] = view.get<Position, Velocity>(entity);
                accum += pos.x + pos.y + pos.z + vel.vx + vel.vy + vel.vz;
            }
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iteration multi component separate (same layout in entt)
    static void iter_separate_multi(benchmark::State& state) {
        iter_grouped_multi(state); // identical in entt
    }

    // Iterate sparse intersection - N entities with Position, N with Velocity, only 1/50 have both
    static void iter_sparse_multi(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        const int step = 50;
        
        // Create N entities with Position
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
        }
        // Add Velocity only to every 50th entity
        int idx = 0;
        for (auto e : reg.view<Position>()) {
            if (idx % step == 0) {
                reg.emplace<Velocity>(e, Velocity{ (float)idx * 0.5f, (float)idx * 0.25f, (float)idx * 0.125f });
            }
            ++idx;
        }
        
        auto view = reg.view<Position, Velocity>();
        for (auto _ : state) {
            float accum = 0.f;
            for (auto entity : view) {
                auto [pos, vel] = view.get<Position, Velocity>(entity);
                accum += pos.x + pos.y + pos.z + vel.vx + vel.vy + vel.vz;
            }
            benchmark::DoNotOptimize(accum);
        }
    }
}

// ================= Flecs benchmarks =====================
namespace flecs {
    using flecs_world = flecs::world;

    static void insert(benchmark::State &state) {
        for (auto _ : state) {
            flecs_world world;
            world.component<Position>();
            for (int i = 0; i < state.range(0); ++i) {
                world.entity().set<Position>({42.f,42.f,42.f});
            }
        }
    }

    static void create_entities(benchmark::State &state) {
        for (auto _ : state) {
            flecs_world world;
            for (int i = 0; i < state.range(0); ++i) {
                benchmark::DoNotOptimize(world.entity());
            }
        }
    }

    static void add_int_component(benchmark::State &state) { // name kept
        for (auto _ : state) {
            flecs_world world;
            world.component<Position>();
            for (int i = 0; i < state.range(0); ++i) {
                world.entity().set<Position>({(float)i, (float)i + 1.f, (float)i + 2.f});
            }
        }
    }

    static void add_struct_component(benchmark::State &state) {
        for (auto _ : state) {
            flecs_world world;
            world.component<Velocity>();
            for (int i = 0; i < state.range(0); ++i) {
                world.entity().set<Velocity>({(float)i, (float)i * 2.f, (float)i * 3.f});
            }
        }
    }

    static void grouped_insert(benchmark::State &state) { // Position + Velocity
        for (auto _ : state) {
            flecs_world world;
            world.component<Position>();
            world.component<Velocity>();
            for (int i = 0; i < state.range(0); ++i) {
                world.entity().set<Position>({7.f,8.f,9.f}).set<Velocity>({1.f,2.f,3.f});
            }
        }
    }

    static void has_component(benchmark::State &state) {
        flecs_world world;
        world.component<Position>();
        std::vector<flecs::entity> ids; ids.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            ids.emplace_back(world.entity().set<Position>({1.f,2.f,3.f}));
        }
        for (auto _ : state) {
            size_t count = 0;
            for (auto &e : ids) {
                if (e.has<Position>()) ++count;
            }
            benchmark::DoNotOptimize(count);
        }
    }

    static void destroy_entities(benchmark::State &state) {
        for (auto _ : state) {
            state.PauseTiming();
            flecs_world world;
            world.component<Position>();
            world.component<Velocity>();
            std::vector<flecs::entity> ids; ids.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                ids.emplace_back(world.entity().set<Position>({0.f,0.f,0.f}).set<Velocity>({0.f,0.f,0.f}));
            }
            state.ResumeTiming();
            // Use defer for batch deletion (similar to ecss/entt batch APIs)
            world.defer_begin();
            for (auto &e : ids) {
                e.destruct();
            }
            world.defer_end();
            benchmark::ClobberMemory();
        }
    }

    static void iter_single_component(benchmark::State &state) {
        flecs_world world;
        world.component<Position>();
        for (int i = 0; i < state.range(0); ++i) {
            world.entity().set<Position>({(float)i, (float)i + 1.f, (float)i + 2.f});
        }
        flecs::query<Position> q = world.query<Position>();
        for (auto _ : state) {
            float sum = 0.f;
            q.each([&](Position &p){ sum += p.x + p.y + p.z; });
            benchmark::DoNotOptimize(sum);
        }
    }

    static void iter_grouped_multi(benchmark::State &state) { // Position + Velocity
        flecs_world world;
        world.component<Position>();
        world.component<Velocity>();
        for (int i = 0; i < state.range(0); ++i) {
            world.entity().set<Position>({(float)i, (float)i * 2.f, (float)i * 3.f})
                             .set<Velocity>({(float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f});
        }
        flecs::query<Position, Velocity> q = world.query<Position, Velocity>();
        for (auto _ : state) {
            float accum = 0.f;
            q.each([&](Position &p, Velocity &v){ accum += p.x + p.y + p.z + v.vx + v.vy + v.vz; });
            benchmark::DoNotOptimize(accum);
        }
    }

    static void iter_separate_multi(benchmark::State &state) { // same layout for flecs
        iter_grouped_multi(state);
    }

    // Iterate sparse intersection - N entities with Position, N with Velocity, only 1/50 have both
    static void iter_sparse_multi(benchmark::State &state) {
        flecs_world world;
        world.component<Position>();
        world.component<Velocity>();
        const int n = state.range(0);
        const int step = 50;
        
        // Create N entities with Position
        std::vector<flecs::entity> entities;
        entities.reserve(n);
        for (int i = 0; i < n; ++i) {
            entities.push_back(world.entity().set<Position>({(float)i, (float)i + 1.f, (float)i + 2.f}));
        }
        // Add Velocity only to every 50th entity
        for (int i = 0; i < n; i += step) {
            entities[i].set<Velocity>({(float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f});
        }
        
        flecs::query<Position, Velocity> q = world.query<Position, Velocity>();
        for (auto _ : state) {
            float accum = 0.f;
            q.each([&](Position &p, Velocity &v){ accum += p.x + p.y + p.z + v.vx + v.vy + v.vz; });
            benchmark::DoNotOptimize(accum);
        }
    }
}

#define TO_FUNC_NAME(funcName, ecs) #ecs "....................." #funcName

#define BENCH_ARGS(F, ECS, FUNC) \
    F(ECS, FUNC, 1000) \
    F(ECS, FUNC, 5000) \
    F(ECS, FUNC, 50000) \
    F(ECS, FUNC, 250000) \
    F(ECS, FUNC, 500000) \
    F(ECS, FUNC, 1000000)

#define BENCH_ONE(ECS, FUNC, ARG) \
    BENCHMARK(ECS::FUNC)->Name(TO_FUNC_NAME(FUNC, ECS))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(ARG);

#define REGISTER_BENCHMARK(ecs0, ecs1, ecs2, ecs3, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs0, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs1, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs2, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs3, FUNC)

REGISTER_BENCHMARK(vec, ecss, entt, flecs, insert)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, create_entities)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, add_int_component)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, add_struct_component)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, grouped_insert)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, has_component)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, destroy_entities)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, iter_single_component)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, iter_grouped_multi)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, iter_separate_multi)
REGISTER_BENCHMARK(vec, ecss, entt, flecs, iter_sparse_multi)

#if ECSS_SINGLE_BENCHS
BENCHMARK(ecss::insert)->Name(TO_FUNC_NAME(insert, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::create_entities)->Name(TO_FUNC_NAME(create_entities, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::add_int_component)->Name(TO_FUNC_NAME(add_int_component, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::add_struct_component)->Name(TO_FUNC_NAME(add_struct_component, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::grouped_insert)->Name(TO_FUNC_NAME(grouped_insert, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::has_component)->Name(TO_FUNC_NAME(has_component, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::destroy_entities)->Name(TO_FUNC_NAME(destroy_entities, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::iter_single_component)->Name(TO_FUNC_NAME(iter_single_component, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::iter_grouped_multi)->Name(TO_FUNC_NAME(iter_grouped_multi, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
BENCHMARK(ecss::iter_separate_multi)->Name(TO_FUNC_NAME(iter_separate_multi, ecss))->Unit(benchmark::TimeUnit::kMillisecond)->Arg(100'000'000);
#endif