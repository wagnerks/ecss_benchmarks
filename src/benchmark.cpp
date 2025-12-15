#include <benchmark/benchmark.h>
#include <entt/entt.hpp>
#include <flecs.h>
#include <ecss/Registry.h>

#include <vector>
#include <unordered_map>
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

    // Iterate multi component (SoA - two separate arrays for fair comparison)
    static void iter_grouped_multi(benchmark::State& state) {
        std::vector<Position> positions;
        std::vector<Velocity> velocities;
        positions.reserve(state.range(0));
        velocities.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            positions.push_back(Position{(float)i, (float)i * 2.f, (float)i * 3.f});
            velocities.push_back(Velocity{(float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f});
        }
        for (auto _ : state) {
            float accum = 0.f;
            for (size_t i = 0; i < positions.size(); ++i) {
                accum += positions[i].x + positions[i].y + positions[i].z +
                         velocities[i].vx + velocities[i].vy + velocities[i].vz;
            }
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate separate - two arrays with entity->index mapping (simulates ECS separate storage)
    static void iter_separate_multi(benchmark::State& state) {
        const int n = state.range(0);
        std::vector<Position> positions(n);
        std::vector<Velocity> velocities(n);
        std::vector<int> entityToIdx(n); // Entity -> component index mapping
        for (int i = 0; i < n; ++i) {
            positions[i] = Position{(float)i, (float)i * 2.f, (float)i * 3.f};
            velocities[i] = Velocity{(float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f};
            entityToIdx[i] = i;
        }
        for (auto _ : state) {
            float accum = 0.f;
            // Iterate with indirection (like ECS separate storage lookup)
            for (int entity = 0; entity < n; ++entity) {
                int idx = entityToIdx[entity];
                accum += positions[idx].x + positions[idx].y + positions[idx].z +
                         velocities[idx].vx + velocities[idx].vy + velocities[idx].vz;
            }
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate sparse intersection - N entities with Position, N/50 with Velocity
    // Must do actual sparse lookup to be fair comparison
    static void iter_sparse_multi(benchmark::State& state) {
        const int n = state.range(0);
        const int step = 50; // every 50th entity has both
        std::vector<Position> positions(n);
        std::unordered_map<int, Velocity> velocityMap; // Sparse storage for velocities
        velocityMap.reserve(n / step);
        
        for (int i = 0; i < n; ++i) {
            positions[i] = Position{(float)i, (float)i * 2.f, (float)i * 3.f};
        }
        for (int i = 0; i < n; i += step) {
            velocityMap[i] = Velocity{(float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f};
        }
        
        for (auto _ : state) {
            float accum = 0.f;
            // Iterate velocity (smaller set) and lookup position
            for (const auto& [entity, vel] : velocityMap) {
                const auto& pos = positions[entity];
                accum += pos.x + pos.y + pos.z + vel.vx + vel.vy + vel.vz;
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
        // Pre-fetch container - same pattern as entt's view caching
        auto* container = reg.getComponentContainer<Position>();
        const auto& layout = container->template getLayoutData<Position>();
        for (auto _ : state) {
            size_t count = 0;
            for (auto id : ids) {
                auto idx = container->template findLinearIdx<false>(id);
                if (idx != ecss::INVALID_IDX && 
                    ecss::Memory::Sector::isAlive(container->template getIsAliveRef<false>(idx), layout.isAliveMask)) {
                    ++count;
                }
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
    // Iterate a single Position component array (using SIMD-optimized each())
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
            view.each([&](Position& p) {
                sum += p.x + p.y + p.z;
            });
            benchmark::DoNotOptimize(sum);
        }
    }

    // Iterate multiple components that are grouped (using SIMD-optimized each())
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
            view.each([&](Position& p, Velocity& v) {
                accum += p.x + p.y + p.z + v.vx + v.vy + v.vz;
            });
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate multiple components in separate arrays (using each() with fallback)
    static void iter_separate_multi(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        Reg reg; // no grouping - will use fallback path
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
            reg.addComponent<Velocity>(e, Velocity{ (float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f });
        }
        auto view = reg.view<Position, Velocity>();
        for (auto _ : state) {
            float accum = 0.f;
            view.each([&](Position& p, Velocity& v) {
                accum += p.x + p.y + p.z + v.vx + v.vy + v.vz;
            });
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate sparse intersection - N entities with Position, N/50 with Velocity
    // Use Velocity as primary (smaller set) for efficient iteration
    // NOTE: NOT grouped - uses random lookup, cache unfriendly
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
            view.each([&](Velocity& v, Position& p) {
                accum += p.x + p.y + p.z + v.vx + v.vy + v.vz;
            });
            benchmark::DoNotOptimize(accum);
        }
    }

}

// ================= Thread-safe ecss benchmarks =====================
namespace ecss_ts
{
    // Insert Position component per entity (constant values)
    static void insert(benchmark::State& state) {
        using Reg = ecss::Registry<true>;
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
        using Reg = ecss::Registry<true>;
        for (auto _ : state) {
            Reg reg;
            for (int i = 0; i < state.range(0); ++i) {
                benchmark::DoNotOptimize(reg.takeEntity());
            }
        }
    }

    // Add Position component with varying values
    static void add_int_component(benchmark::State& state) {
        using Reg = ecss::Registry<true>;
        for (auto _ : state) {
            Reg reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
            }
        }
    }

    // Add Velocity component
    static void add_struct_component(benchmark::State& state) {
        using Reg = ecss::Registry<true>;
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
        using Reg = ecss::Registry<true>;
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
        using Reg = ecss::Registry<true>;
        using ecss::EntityId;
        Reg reg;
        std::vector<EntityId> ids;
        ids.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{1.f,2.f,3.f});
            ids.push_back(e);
        }
        // Pre-fetch container - same pattern as entt's view caching
        auto* container = reg.getComponentContainer<Position>();
        const auto& layout = container->template getLayoutData<Position>();
        for (auto _ : state) {
            size_t count = 0;
            for (auto id : ids) {
                auto idx = container->template findLinearIdx<true>(id);
                if (idx != ecss::INVALID_IDX && 
                    ecss::Memory::Sector::isAlive(container->template getIsAliveRef<true>(idx), layout.isAliveMask)) {
                    ++count;
                }
            }
            benchmark::DoNotOptimize(count);
        }
    }

    // Batch destroy entities (Position + Velocity)
    static void destroy_entities(benchmark::State& state) {
        using Reg = ecss::Registry<true>;
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
        using Reg = ecss::Registry<true>;
        Reg reg;
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
        }
        auto view = reg.view<Position>();
        for (auto _ : state) {
            float sum = 0.f;
            view.each([&](Position& p) {
                sum += p.x + p.y + p.z;
            });
            benchmark::DoNotOptimize(sum);
        }
    }

    // Iterate multiple components that are grouped
    static void iter_grouped_multi(benchmark::State& state) {
        using Reg = ecss::Registry<true>;
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
            view.each([&](Position& p, Velocity& v) {
                accum += p.x + p.y + p.z + v.vx + v.vy + v.vz;
            });
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate multiple components in separate arrays
    static void iter_separate_multi(benchmark::State& state) {
        using Reg = ecss::Registry<true>;
        Reg reg; // no grouping - will use fallback path
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
            reg.addComponent<Velocity>(e, Velocity{ (float)i * 0.5f, (float)i * 0.25f, (float)i * 0.125f });
        }
        auto view = reg.view<Position, Velocity>();
        for (auto _ : state) {
            float accum = 0.f;
            view.each([&](Position& p, Velocity& v) {
                accum += p.x + p.y + p.z + v.vx + v.vy + v.vz;
            });
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate sparse intersection - N entities with Position, N/50 with Velocity
    static void iter_sparse_multi(benchmark::State& state) {
        using Reg = ecss::Registry<true>;
        Reg reg;
        const int n = state.range(0);
        const int step = 50;
        
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
            view.each([&](Velocity& v, Position& p) {
                accum += p.x + p.y + p.z + v.vx + v.vy + v.vz;
            });
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
            view.each([&](Position& p) {
                sum += p.x + p.y + p.z;
            });
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
            view.each([&](Position& pos, Velocity& vel) {
                accum += pos.x + pos.y + pos.z + vel.vx + vel.vy + vel.vz;
            });
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
            view.each([&](Position& pos, Velocity& vel) {
                accum += pos.x + pos.y + pos.z + vel.vx + vel.vy + vel.vz;
            });
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
    BENCHMARK(ECS::FUNC)->Name(TO_FUNC_NAME(FUNC, ECS))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(ARG)->MinTime(0.3);

// MSVC has issues with std::atomic::wait()/notify_all() used in ecss_ts (thread-safe version)
// Skip ecss_ts benchmarks on Windows to avoid hangs/crashes
#ifdef _MSC_VER
#define REGISTER_BENCHMARK(ecs0, ecs1, ecs2, ecs3, ecs4, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs0, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs1, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs3, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs4, FUNC)
#else
#define REGISTER_BENCHMARK(ecs0, ecs1, ecs2, ecs3, ecs4, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs0, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs1, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs2, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs3, FUNC) \
    BENCH_ARGS(BENCH_ONE, ecs4, FUNC)
#endif

REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, insert)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, create_entities)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, add_int_component)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, add_struct_component)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, grouped_insert)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, has_component)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, destroy_entities)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, iter_single_component)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, iter_grouped_multi)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, iter_separate_multi)
REGISTER_BENCHMARK(vec, ecss, ecss_ts, entt, flecs, iter_sparse_multi)

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

// =====================================================================
// REALISTIC GAME-LIKE BENCHMARK SCENARIOS
// =====================================================================

// Additional components for realistic scenarios
struct Transform {
    float x, y, z;           // position
    float rx, ry, rz, rw;    // rotation quaternion
    float sx, sy, sz;        // scale
};

struct RigidBody {
    float vx, vy, vz;        // velocity
    float ax, ay, az;        // acceleration
    float mass;
    float drag;
};

struct Health {
    float current;
    float max;
    float regen;
    bool isDead;
};

struct Damage {
    float amount;
    float armor;
    float critChance;
    float critMultiplier;
};

struct AIState {
    int state;               // 0=idle, 1=patrol, 2=chase, 3=attack
    float timer;
    float aggroRange;
    float attackRange;
    uint32_t targetEntity;
};

struct Sprite {
    uint32_t textureId;
    float u0, v0, u1, v1;    // UV coords
    uint32_t color;
    int layer;
};

struct ParticleEmitter {
    float emitRate;
    float lifetime;
    float timer;
    int maxParticles;
    int activeParticles;
};

struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

struct Tag_Player {};
struct Tag_Enemy {};
struct Tag_Projectile {};
struct Tag_Static {};

// =====================================================================
// Scenario 1: Physics System - Position/Velocity/Acceleration integration
// Typical: 10k-100k entities (particles, projectiles, physics objects)
// =====================================================================

namespace realistic {
namespace ecss_r {
    using Reg = ecss::Registry<false>;

    // Physics integration: pos += vel * dt; vel += acc * dt
    static void physics_integration(benchmark::State& state) {
        Reg reg;
        reg.registerArray<Transform, RigidBody>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Transform>(e, Transform{
                (float)i, (float)(i * 2), 0.f,
                0.f, 0.f, 0.f, 1.f,
                1.f, 1.f, 1.f
            });
            reg.addComponent<RigidBody>(e, RigidBody{
                1.f, 0.5f, 0.f,   // velocity
                0.f, -9.8f, 0.f, // gravity
                1.f, 0.1f
            });
        }
        
        const float dt = 1.f / 60.f;
        auto view = reg.view<Transform, RigidBody>();
        
        for (auto _ : state) {
            view.each([dt](Transform& t, RigidBody& rb) {
                // Integrate velocity
                rb.vx += rb.ax * dt;
                rb.vy += rb.ay * dt;
                rb.vz += rb.az * dt;
                
                // Apply drag
                rb.vx *= (1.f - rb.drag * dt);
                rb.vy *= (1.f - rb.drag * dt);
                rb.vz *= (1.f - rb.drag * dt);
                
                // Integrate position
                t.x += rb.vx * dt;
                t.y += rb.vy * dt;
                t.z += rb.vz * dt;
            });
            benchmark::ClobberMemory();
        }
    }

    // Health regeneration system
    static void health_regen(benchmark::State& state) {
        Reg reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Health>(e, Health{
                50.f + (float)(i % 50),  // current
                100.f,                    // max
                1.f + (float)(i % 5),     // regen rate
                false
            });
        }
        
        const float dt = 1.f / 60.f;
        auto view = reg.view<Health>();
        
        for (auto _ : state) {
            view.each([dt](Health& h) {
                if (!h.isDead && h.current < h.max) {
                    h.current = std::min(h.max, h.current + h.regen * dt);
                }
            });
            benchmark::ClobberMemory();
        }
    }

    // AI state machine - complex branching logic
    static void ai_state_machine(benchmark::State& state) {
        Reg reg;
        reg.registerArray<Transform, AIState>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Transform>(e, Transform{
                (float)(i % 1000), (float)(i / 1000), 0.f,
                0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f
            });
            reg.addComponent<AIState>(e, AIState{
                i % 4,           // state
                (float)(i % 60) / 60.f, // timer
                100.f,           // aggro range
                20.f,            // attack range
                0                // target
            });
        }
        
        // Simulated player position for distance check
        const float playerX = 500.f, playerY = 500.f;
        const float dt = 1.f / 60.f;
        auto view = reg.view<Transform, AIState>();
        
        for (auto _ : state) {
            view.each([&](Transform& t, AIState& ai) {
                ai.timer -= dt;
                
                float dx = playerX - t.x;
                float dy = playerY - t.y;
                float distSq = dx * dx + dy * dy;
                
                switch (ai.state) {
                    case 0: // idle
                        if (distSq < ai.aggroRange * ai.aggroRange) {
                            ai.state = 2; // chase
                        } else if (ai.timer <= 0.f) {
                            ai.state = 1; // patrol
                            ai.timer = 3.f;
                        }
                        break;
                    case 1: // patrol
                        if (distSq < ai.aggroRange * ai.aggroRange) {
                            ai.state = 2;
                        } else if (ai.timer <= 0.f) {
                            ai.state = 0;
                            ai.timer = 2.f;
                        }
                        break;
                    case 2: // chase
                        if (distSq < ai.attackRange * ai.attackRange) {
                            ai.state = 3; // attack
                            ai.timer = 1.f;
                        } else if (distSq > ai.aggroRange * ai.aggroRange * 1.5f) {
                            ai.state = 0;
                        }
                        break;
                    case 3: // attack
                        if (ai.timer <= 0.f) {
                            ai.state = 2; // back to chase
                        }
                        break;
                }
            });
            benchmark::ClobberMemory();
        }
    }

    // Sprite batching - prepare render data
    static void sprite_batching(benchmark::State& state) {
        Reg reg;
        reg.registerArray<Transform, Sprite>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Transform>(e, Transform{
                (float)(i % 1920), (float)((i / 1920) % 1080), 0.f,
                0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f
            });
            reg.addComponent<Sprite>(e, Sprite{
                (uint32_t)(i % 256),  // texture
                0.f, 0.f, 1.f, 1.f,
                0xFFFFFFFF,
                i % 10
            });
        }
        
        auto view = reg.view<Transform, Sprite>();
        
        // Simulate batch buffer (just accumulate, don't actually render)
        struct BatchVertex { float x, y, u, v; uint32_t color; };
        std::vector<BatchVertex> batch;
        batch.reserve(n * 4); // 4 vertices per sprite
        
        for (auto _ : state) {
            batch.clear();
            view.each([&](Transform& t, Sprite& s) {
                // Generate quad vertices
                batch.push_back({t.x,        t.y,        s.u0, s.v0, s.color});
                batch.push_back({t.x + t.sx, t.y,        s.u1, s.v0, s.color});
                batch.push_back({t.x + t.sx, t.y + t.sy, s.u1, s.v1, s.color});
                batch.push_back({t.x,        t.y + t.sy, s.u0, s.v1, s.color});
            });
            benchmark::DoNotOptimize(batch.data());
        }
    }

    // Particle system update
    static void particle_system(benchmark::State& state) {
        Reg reg;
        reg.registerArray<Position, Velocity>();
        const int n = state.range(0);
        
        // Create particles
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            float angle = (float)(i % 360) * 3.14159f / 180.f;
            float speed = 50.f + (float)(i % 100);
            reg.addComponent<Position>(e, Position{
                (float)(i % 100), (float)((i / 100) % 100), 0.f
            });
            reg.addComponent<Velocity>(e, Velocity{
                std::cos(angle) * speed,
                std::sin(angle) * speed,
                0.f
            });
        }
        
        const float dt = 1.f / 60.f;
        auto view = reg.view<Position, Velocity>();
        
        for (auto _ : state) {
            view.each([dt](Position& p, Velocity& v) {
                // Simple physics
                v.vy -= 98.f * dt; // gravity
                p.x += v.vx * dt;
                p.y += v.vy * dt;
                p.z += v.vz * dt;
                
                // Damping
                v.vx *= 0.99f;
                v.vy *= 0.99f;
                v.vz *= 0.99f;
            });
            benchmark::ClobberMemory();
        }
    }

    // Combat system - damage calculation with armor
    static void combat_damage(benchmark::State& state) {
        Reg reg;
        reg.registerArray<Health, Damage>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Health>(e, Health{
                100.f, 100.f, 0.f, false
            });
            reg.addComponent<Damage>(e, Damage{
                10.f + (float)(i % 20),  // damage
                5.f + (float)(i % 10),   // armor
                0.1f + (float)(i % 10) / 100.f, // crit chance
                2.f                       // crit multiplier
            });
        }
        
        auto view = reg.view<Health, Damage>();
        
        // Pseudo-random for benchmark consistency
        uint32_t seed = 12345;
        auto pseudoRandom = [&seed]() -> float {
            seed = seed * 1103515245 + 12345;
            return (float)(seed % 1000) / 1000.f;
        };
        
        for (auto _ : state) {
            view.each([&](Health& h, Damage& d) {
                if (h.isDead) return;
                
                float finalDamage = d.amount - d.armor * 0.5f;
                if (finalDamage < 1.f) finalDamage = 1.f;
                
                // Crit check
                if (pseudoRandom() < d.critChance) {
                    finalDamage *= d.critMultiplier;
                }
                
                h.current -= finalDamage;
                if (h.current <= 0.f) {
                    h.current = 0.f;
                    h.isDead = true;
                }
            });
            benchmark::ClobberMemory();
            
            // Reset for next iteration
            state.PauseTiming();
            view.each([](Health& h, Damage&) {
                h.current = h.max;
                h.isDead = false;
            });
            state.ResumeTiming();
        }
    }

    // AABB collision broad phase (count pairs)
    static void collision_broadphase(benchmark::State& state) {
        Reg reg;
        reg.registerArray<Transform, AABB>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            float x = (float)(i % 100) * 10.f;
            float y = (float)((i / 100) % 100) * 10.f;
            float z = (float)(i / 10000) * 10.f;
            reg.addComponent<Transform>(e, Transform{
                x, y, z,
                0.f, 0.f, 0.f, 1.f,
                1.f, 1.f, 1.f
            });
            reg.addComponent<AABB>(e, AABB{
                x - 1.f, y - 1.f, z - 1.f,
                x + 1.f, y + 1.f, z + 1.f
            });
        }
        
        auto view = reg.view<AABB>();
        
        for (auto _ : state) {
            // Just update AABBs from transforms and count potential overlaps
            size_t overlaps = 0;
            float lastMaxX = -1e9f;
            
            // Simple sweep (assumes sorted by X, which they roughly are)
            view.each([&](AABB& a) {
                if (a.minX < lastMaxX) {
                    overlaps++;
                }
                lastMaxX = std::max(lastMaxX, a.maxX);
            });
            benchmark::DoNotOptimize(overlaps);
        }
    }

    // Entity spawn/despawn churn - simulates projectile lifecycle
    static void entity_churn(benchmark::State& state) {
        Reg reg;
        reg.registerArray<Position, Velocity>();
        const int n = state.range(0);
        const int churnRate = n / 10; // 10% churn per frame
        
        std::vector<ecss::EntityId> entities;
        entities.reserve(n);
        
        // Initial population
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{(float)i, 0.f, 0.f});
            reg.addComponent<Velocity>(e, Velocity{1.f, 0.f, 0.f});
            entities.push_back(e);
        }
        
        int frameCounter = 0;
        for (auto _ : state) {
            // Destroy oldest entities
            std::vector<ecss::EntityId> toDestroy;
            toDestroy.reserve(churnRate);
            for (int i = 0; i < churnRate && !entities.empty(); ++i) {
                toDestroy.push_back(entities[i]);
            }
            if (!toDestroy.empty()) {
                reg.destroyEntities(toDestroy);
                entities.erase(entities.begin(), entities.begin() + toDestroy.size());
            }
            
            // Spawn new entities
            for (int i = 0; i < churnRate; ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<Position>(e, Position{(float)frameCounter, (float)i, 0.f});
                reg.addComponent<Velocity>(e, Velocity{1.f, 0.f, 0.f});
                entities.push_back(e);
            }
            
            // Update physics
            auto view = reg.view<Position, Velocity>();
            view.each([](Position& p, Velocity& v) {
                p.x += v.vx;
                p.y += v.vy;
                p.z += v.vz;
            });
            
            frameCounter++;
            benchmark::ClobberMemory();
        }
    }

    // Mixed archetype iteration - different entity "types"
    // 40% have Transform only, 30% have Transform+Sprite, 
    // 20% have Transform+RigidBody, 10% have all three
    static void mixed_archetypes(benchmark::State& state) {
        Reg reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            int type = i % 10;
            
            reg.addComponent<Transform>(e, Transform{
                (float)i, 0.f, 0.f,
                0.f, 0.f, 0.f, 1.f,
                1.f, 1.f, 1.f
            });
            
            if (type >= 4) { // 60% have Sprite
                reg.addComponent<Sprite>(e, Sprite{
                    (uint32_t)(i % 256), 0.f, 0.f, 1.f, 1.f, 0xFFFFFFFF, 0
                });
            }
            if (type >= 7 || type < 2) { // 50% have RigidBody
                reg.addComponent<RigidBody>(e, RigidBody{
                    1.f, 0.f, 0.f, 0.f, -9.8f, 0.f, 1.f, 0.1f
                });
            }
        }
        
        const float dt = 1.f / 60.f;
        
        for (auto _ : state) {
            // Physics system (Transform + RigidBody)
            {
                auto view = reg.view<Transform, RigidBody>();
                view.each([dt](Transform& t, RigidBody& rb) {
                    rb.vy += rb.ay * dt;
                    t.x += rb.vx * dt;
                    t.y += rb.vy * dt;
                });
            }
            
            // Render prep (Transform + Sprite)
            {
                float accum = 0.f;
                auto view = reg.view<Transform, Sprite>();
                view.each([&](Transform& t, Sprite& s) {
                    accum += t.x * (float)s.layer;
                });
                benchmark::DoNotOptimize(accum);
            }
            
            benchmark::ClobberMemory();
        }
    }

    // Add/remove component benchmark - tests archetype migration cost
    // Critical for open world: picking items, entering zones, state changes
    static void add_remove_component(benchmark::State& state) {
        Reg reg;
        const int n = state.range(0);
        
        // Create entities with just Position
        std::vector<ecss::EntityId> entities;
        entities.reserve(n);
        for (int i = 0; i < n; ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<Position>(e, Position{(float)i, 0.f, 0.f});
            entities.push_back(e);
        }
        
        bool hasVelocity = false;
        for (auto _ : state) {
            if (!hasVelocity) {
                // Add Velocity to all entities
                for (auto e : entities) {
                    reg.addComponent<Velocity>(e, Velocity{1.f, 2.f, 3.f});
                }
            } else {
                // Remove Velocity from all entities
                for (auto e : entities) {
                    reg.destroyComponent<Velocity>(e);
                }
            }
            hasVelocity = !hasVelocity;
            benchmark::ClobberMemory();
        }
    }

} // namespace ecss_r

namespace entt_r {
    using big_registry = entt::registry;

    static void physics_integration(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            reg.emplace<Transform>(e, Transform{
                (float)i, (float)(i * 2), 0.f,
                0.f, 0.f, 0.f, 1.f,
                1.f, 1.f, 1.f
            });
            reg.emplace<RigidBody>(e, RigidBody{
                1.f, 0.5f, 0.f,
                0.f, -9.8f, 0.f,
                1.f, 0.1f
            });
        }
        
        const float dt = 1.f / 60.f;
        auto view = reg.view<Transform, RigidBody>();
        
        for (auto _ : state) {
            view.each([dt](Transform& t, RigidBody& rb) {
                rb.vx += rb.ax * dt;
                rb.vy += rb.ay * dt;
                rb.vz += rb.az * dt;
                rb.vx *= (1.f - rb.drag * dt);
                rb.vy *= (1.f - rb.drag * dt);
                rb.vz *= (1.f - rb.drag * dt);
                t.x += rb.vx * dt;
                t.y += rb.vy * dt;
                t.z += rb.vz * dt;
            });
            benchmark::ClobberMemory();
        }
    }

    static void health_regen(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            reg.emplace<Health>(e, Health{
                50.f + (float)(i % 50), 100.f, 1.f + (float)(i % 5), false
            });
        }
        
        const float dt = 1.f / 60.f;
        auto view = reg.view<Health>();
        
        for (auto _ : state) {
            view.each([dt](Health& h) {
                if (!h.isDead && h.current < h.max) {
                    h.current = std::min(h.max, h.current + h.regen * dt);
                }
            });
            benchmark::ClobberMemory();
        }
    }

    static void ai_state_machine(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            reg.emplace<Transform>(e, Transform{
                (float)(i % 1000), (float)(i / 1000), 0.f,
                0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f
            });
            reg.emplace<AIState>(e, AIState{
                i % 4, (float)(i % 60) / 60.f, 100.f, 20.f, 0
            });
        }
        
        const float playerX = 500.f, playerY = 500.f;
        const float dt = 1.f / 60.f;
        auto view = reg.view<Transform, AIState>();
        
        for (auto _ : state) {
            view.each([&](Transform& t, AIState& ai) {
                ai.timer -= dt;
                float dx = playerX - t.x;
                float dy = playerY - t.y;
                float distSq = dx * dx + dy * dy;
                
                switch (ai.state) {
                    case 0:
                        if (distSq < ai.aggroRange * ai.aggroRange) ai.state = 2;
                        else if (ai.timer <= 0.f) { ai.state = 1; ai.timer = 3.f; }
                        break;
                    case 1:
                        if (distSq < ai.aggroRange * ai.aggroRange) ai.state = 2;
                        else if (ai.timer <= 0.f) { ai.state = 0; ai.timer = 2.f; }
                        break;
                    case 2:
                        if (distSq < ai.attackRange * ai.attackRange) { ai.state = 3; ai.timer = 1.f; }
                        else if (distSq > ai.aggroRange * ai.aggroRange * 1.5f) ai.state = 0;
                        break;
                    case 3:
                        if (ai.timer <= 0.f) ai.state = 2;
                        break;
                }
            });
            benchmark::ClobberMemory();
        }
    }

    static void sprite_batching(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            reg.emplace<Transform>(e, Transform{
                (float)(i % 1920), (float)((i / 1920) % 1080), 0.f,
                0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f
            });
            reg.emplace<Sprite>(e, Sprite{
                (uint32_t)(i % 256), 0.f, 0.f, 1.f, 1.f, 0xFFFFFFFF, i % 10
            });
        }
        
        auto view = reg.view<Transform, Sprite>();
        struct BatchVertex { float x, y, u, v; uint32_t color; };
        std::vector<BatchVertex> batch;
        batch.reserve(n * 4);
        
        for (auto _ : state) {
            batch.clear();
            view.each([&](Transform& t, Sprite& s) {
                batch.push_back({t.x, t.y, s.u0, s.v0, s.color});
                batch.push_back({t.x + t.sx, t.y, s.u1, s.v0, s.color});
                batch.push_back({t.x + t.sx, t.y + t.sy, s.u1, s.v1, s.color});
                batch.push_back({t.x, t.y + t.sy, s.u0, s.v1, s.color});
            });
            benchmark::DoNotOptimize(batch.data());
        }
    }

    static void particle_system(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            float angle = (float)(i % 360) * 3.14159f / 180.f;
            float speed = 50.f + (float)(i % 100);
            reg.emplace<Position>(e, Position{(float)(i % 100), (float)((i / 100) % 100), 0.f});
            reg.emplace<Velocity>(e, Velocity{std::cos(angle) * speed, std::sin(angle) * speed, 0.f});
        }
        
        const float dt = 1.f / 60.f;
        auto view = reg.view<Position, Velocity>();
        
        for (auto _ : state) {
            view.each([dt](Position& p, Velocity& v) {
                v.vy -= 98.f * dt;
                p.x += v.vx * dt;
                p.y += v.vy * dt;
                p.z += v.vz * dt;
                v.vx *= 0.99f;
                v.vy *= 0.99f;
                v.vz *= 0.99f;
            });
            benchmark::ClobberMemory();
        }
    }

    static void combat_damage(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            reg.emplace<Health>(e, Health{100.f, 100.f, 0.f, false});
            reg.emplace<Damage>(e, Damage{
                10.f + (float)(i % 20), 5.f + (float)(i % 10),
                0.1f + (float)(i % 10) / 100.f, 2.f
            });
        }
        
        auto view = reg.view<Health, Damage>();
        uint32_t seed = 12345;
        auto pseudoRandom = [&seed]() -> float {
            seed = seed * 1103515245 + 12345;
            return (float)(seed % 1000) / 1000.f;
        };
        
        for (auto _ : state) {
            view.each([&](Health& h, Damage& d) {
                if (h.isDead) return;
                float finalDamage = d.amount - d.armor * 0.5f;
                if (finalDamage < 1.f) finalDamage = 1.f;
                if (pseudoRandom() < d.critChance) finalDamage *= d.critMultiplier;
                h.current -= finalDamage;
                if (h.current <= 0.f) { h.current = 0.f; h.isDead = true; }
            });
            benchmark::ClobberMemory();
            
            state.PauseTiming();
            view.each([](Health& h, Damage&) { h.current = h.max; h.isDead = false; });
            state.ResumeTiming();
        }
    }

    static void collision_broadphase(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            float x = (float)(i % 100) * 10.f;
            float y = (float)((i / 100) % 100) * 10.f;
            float z = (float)(i / 10000) * 10.f;
            reg.emplace<Transform>(e, Transform{x, y, z, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f});
            reg.emplace<AABB>(e, AABB{x - 1.f, y - 1.f, z - 1.f, x + 1.f, y + 1.f, z + 1.f});
        }
        
        auto view = reg.view<AABB>();
        
        for (auto _ : state) {
            size_t overlaps = 0;
            float lastMaxX = -1e9f;
            view.each([&](AABB& a) {
                if (a.minX < lastMaxX) overlaps++;
                lastMaxX = std::max(lastMaxX, a.maxX);
            });
            benchmark::DoNotOptimize(overlaps);
        }
    }

    static void entity_churn(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        const int churnRate = n / 10;
        
        std::vector<entt::entity> entities;
        entities.reserve(n);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, Position{(float)i, 0.f, 0.f});
            reg.emplace<Velocity>(e, Velocity{1.f, 0.f, 0.f});
            entities.push_back(e);
        }
        
        int frameCounter = 0;
        for (auto _ : state) {
            // Destroy oldest
            for (int i = 0; i < churnRate && !entities.empty(); ++i) {
                reg.destroy(entities[i]);
            }
            entities.erase(entities.begin(), entities.begin() + std::min(churnRate, (int)entities.size()));
            
            // Spawn new
            for (int i = 0; i < churnRate; ++i) {
                auto e = reg.create();
                reg.emplace<Position>(e, Position{(float)frameCounter, (float)i, 0.f});
                reg.emplace<Velocity>(e, Velocity{1.f, 0.f, 0.f});
                entities.push_back(e);
            }
            
            auto view = reg.view<Position, Velocity>();
            view.each([](Position& p, Velocity& v) {
                p.x += v.vx; p.y += v.vy; p.z += v.vz;
            });
            
            frameCounter++;
            benchmark::ClobberMemory();
        }
    }

    static void mixed_archetypes(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            int type = i % 10;
            reg.emplace<Transform>(e, Transform{(float)i, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f});
            if (type >= 4) reg.emplace<Sprite>(e, Sprite{(uint32_t)(i % 256), 0.f, 0.f, 1.f, 1.f, 0xFFFFFFFF, 0});
            if (type >= 7 || type < 2) reg.emplace<RigidBody>(e, RigidBody{1.f, 0.f, 0.f, 0.f, -9.8f, 0.f, 1.f, 0.1f});
        }
        
        const float dt = 1.f / 60.f;
        
        for (auto _ : state) {
            {
                auto view = reg.view<Transform, RigidBody>();
                view.each([dt](Transform& t, RigidBody& rb) {
                    rb.vy += rb.ay * dt;
                    t.x += rb.vx * dt;
                    t.y += rb.vy * dt;
                });
            }
            {
                float accum = 0.f;
                auto view = reg.view<Transform, Sprite>();
                view.each([&](Transform& t, Sprite& s) { accum += t.x * (float)s.layer; });
                benchmark::DoNotOptimize(accum);
            }
            benchmark::ClobberMemory();
        }
    }

    // Add/remove component benchmark
    static void add_remove_component(benchmark::State& state) {
        big_registry reg;
        const int n = state.range(0);
        
        std::vector<entt::entity> entities;
        entities.reserve(n);
        for (int i = 0; i < n; ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, Position{(float)i, 0.f, 0.f});
            entities.push_back(e);
        }
        
        bool hasVelocity = false;
        for (auto _ : state) {
            if (!hasVelocity) {
                for (auto e : entities) {
                    reg.emplace<Velocity>(e, Velocity{1.f, 2.f, 3.f});
                }
            } else {
                for (auto e : entities) {
                    reg.remove<Velocity>(e);
                }
            }
            hasVelocity = !hasVelocity;
            benchmark::ClobberMemory();
        }
    }

} // namespace entt_r

namespace flecs_r {
    using flecs_world = flecs::world;

    static void physics_integration(benchmark::State& state) {
        flecs_world world;
        world.component<Transform>();
        world.component<RigidBody>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            world.entity()
                .set<Transform>({(float)i, (float)(i * 2), 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f})
                .set<RigidBody>({1.f, 0.5f, 0.f, 0.f, -9.8f, 0.f, 1.f, 0.1f});
        }
        
        const float dt = 1.f / 60.f;
        auto q = world.query<Transform, RigidBody>();
        
        for (auto _ : state) {
            q.each([dt](Transform& t, RigidBody& rb) {
                rb.vx += rb.ax * dt;
                rb.vy += rb.ay * dt;
                rb.vz += rb.az * dt;
                rb.vx *= (1.f - rb.drag * dt);
                rb.vy *= (1.f - rb.drag * dt);
                rb.vz *= (1.f - rb.drag * dt);
                t.x += rb.vx * dt;
                t.y += rb.vy * dt;
                t.z += rb.vz * dt;
            });
            benchmark::ClobberMemory();
        }
    }

    static void health_regen(benchmark::State& state) {
        flecs_world world;
        world.component<Health>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            world.entity().set<Health>({50.f + (float)(i % 50), 100.f, 1.f + (float)(i % 5), false});
        }
        
        const float dt = 1.f / 60.f;
        auto q = world.query<Health>();
        
        for (auto _ : state) {
            q.each([dt](Health& h) {
                if (!h.isDead && h.current < h.max) {
                    h.current = std::min(h.max, h.current + h.regen * dt);
                }
            });
            benchmark::ClobberMemory();
        }
    }

    static void ai_state_machine(benchmark::State& state) {
        flecs_world world;
        world.component<Transform>();
        world.component<AIState>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            world.entity()
                .set<Transform>({(float)(i % 1000), (float)(i / 1000), 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f})
                .set<AIState>({i % 4, (float)(i % 60) / 60.f, 100.f, 20.f, 0});
        }
        
        const float playerX = 500.f, playerY = 500.f;
        const float dt = 1.f / 60.f;
        auto q = world.query<Transform, AIState>();
        
        for (auto _ : state) {
            q.each([&](Transform& t, AIState& ai) {
                ai.timer -= dt;
                float dx = playerX - t.x;
                float dy = playerY - t.y;
                float distSq = dx * dx + dy * dy;
                
                switch (ai.state) {
                    case 0:
                        if (distSq < ai.aggroRange * ai.aggroRange) ai.state = 2;
                        else if (ai.timer <= 0.f) { ai.state = 1; ai.timer = 3.f; }
                        break;
                    case 1:
                        if (distSq < ai.aggroRange * ai.aggroRange) ai.state = 2;
                        else if (ai.timer <= 0.f) { ai.state = 0; ai.timer = 2.f; }
                        break;
                    case 2:
                        if (distSq < ai.attackRange * ai.attackRange) { ai.state = 3; ai.timer = 1.f; }
                        else if (distSq > ai.aggroRange * ai.aggroRange * 1.5f) ai.state = 0;
                        break;
                    case 3:
                        if (ai.timer <= 0.f) ai.state = 2;
                        break;
                }
            });
            benchmark::ClobberMemory();
        }
    }

    static void sprite_batching(benchmark::State& state) {
        flecs_world world;
        world.component<Transform>();
        world.component<Sprite>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            world.entity()
                .set<Transform>({(float)(i % 1920), (float)((i / 1920) % 1080), 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f})
                .set<Sprite>({(uint32_t)(i % 256), 0.f, 0.f, 1.f, 1.f, 0xFFFFFFFF, i % 10});
        }
        
        auto q = world.query<Transform, Sprite>();
        struct BatchVertex { float x, y, u, v; uint32_t color; };
        std::vector<BatchVertex> batch;
        batch.reserve(n * 4);
        
        for (auto _ : state) {
            batch.clear();
            q.each([&](Transform& t, Sprite& s) {
                batch.push_back({t.x, t.y, s.u0, s.v0, s.color});
                batch.push_back({t.x + t.sx, t.y, s.u1, s.v0, s.color});
                batch.push_back({t.x + t.sx, t.y + t.sy, s.u1, s.v1, s.color});
                batch.push_back({t.x, t.y + t.sy, s.u0, s.v1, s.color});
            });
            benchmark::DoNotOptimize(batch.data());
        }
    }

    static void particle_system(benchmark::State& state) {
        flecs_world world;
        world.component<Position>();
        world.component<Velocity>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            float angle = (float)(i % 360) * 3.14159f / 180.f;
            float speed = 50.f + (float)(i % 100);
            world.entity()
                .set<Position>({(float)(i % 100), (float)((i / 100) % 100), 0.f})
                .set<Velocity>({std::cos(angle) * speed, std::sin(angle) * speed, 0.f});
        }
        
        const float dt = 1.f / 60.f;
        auto q = world.query<Position, Velocity>();
        
        for (auto _ : state) {
            q.each([dt](Position& p, Velocity& v) {
                v.vy -= 98.f * dt;
                p.x += v.vx * dt;
                p.y += v.vy * dt;
                p.z += v.vz * dt;
                v.vx *= 0.99f;
                v.vy *= 0.99f;
                v.vz *= 0.99f;
            });
            benchmark::ClobberMemory();
        }
    }

    static void combat_damage(benchmark::State& state) {
        flecs_world world;
        world.component<Health>();
        world.component<Damage>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            world.entity()
                .set<Health>({100.f, 100.f, 0.f, false})
                .set<Damage>({10.f + (float)(i % 20), 5.f + (float)(i % 10), 0.1f + (float)(i % 10) / 100.f, 2.f});
        }
        
        auto q = world.query<Health, Damage>();
        uint32_t seed = 12345;
        auto pseudoRandom = [&seed]() -> float {
            seed = seed * 1103515245 + 12345;
            return (float)(seed % 1000) / 1000.f;
        };
        
        for (auto _ : state) {
            q.each([&](Health& h, Damage& d) {
                if (h.isDead) return;
                float finalDamage = d.amount - d.armor * 0.5f;
                if (finalDamage < 1.f) finalDamage = 1.f;
                if (pseudoRandom() < d.critChance) finalDamage *= d.critMultiplier;
                h.current -= finalDamage;
                if (h.current <= 0.f) { h.current = 0.f; h.isDead = true; }
            });
            benchmark::ClobberMemory();
            
            state.PauseTiming();
            q.each([](Health& h, Damage&) { h.current = h.max; h.isDead = false; });
            state.ResumeTiming();
        }
    }

    static void collision_broadphase(benchmark::State& state) {
        flecs_world world;
        world.component<Transform>();
        world.component<AABB>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            float x = (float)(i % 100) * 10.f;
            float y = (float)((i / 100) % 100) * 10.f;
            float z = (float)(i / 10000) * 10.f;
            world.entity()
                .set<Transform>({x, y, z, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f})
                .set<AABB>({x - 1.f, y - 1.f, z - 1.f, x + 1.f, y + 1.f, z + 1.f});
        }
        
        auto q = world.query<AABB>();
        
        for (auto _ : state) {
            size_t overlaps = 0;
            float lastMaxX = -1e9f;
            q.each([&](AABB& a) {
                if (a.minX < lastMaxX) overlaps++;
                lastMaxX = std::max(lastMaxX, a.maxX);
            });
            benchmark::DoNotOptimize(overlaps);
        }
    }

    static void entity_churn(benchmark::State& state) {
        flecs_world world;
        world.component<Position>();
        world.component<Velocity>();
        const int n = state.range(0);
        const int churnRate = n / 10;
        
        std::vector<flecs::entity> entities;
        entities.reserve(n);
        
        for (int i = 0; i < n; ++i) {
            entities.push_back(world.entity()
                .set<Position>({(float)i, 0.f, 0.f})
                .set<Velocity>({1.f, 0.f, 0.f}));
        }
        
        auto q = world.query<Position, Velocity>();
        int frameCounter = 0;
        
        for (auto _ : state) {
            world.defer_begin();
            for (int i = 0; i < churnRate && !entities.empty(); ++i) {
                entities[i].destruct();
            }
            world.defer_end();
            entities.erase(entities.begin(), entities.begin() + std::min(churnRate, (int)entities.size()));
            
            for (int i = 0; i < churnRate; ++i) {
                entities.push_back(world.entity()
                    .set<Position>({(float)frameCounter, (float)i, 0.f})
                    .set<Velocity>({1.f, 0.f, 0.f}));
            }
            
            q.each([](Position& p, Velocity& v) {
                p.x += v.vx; p.y += v.vy; p.z += v.vz;
            });
            
            frameCounter++;
            benchmark::ClobberMemory();
        }
    }

    static void mixed_archetypes(benchmark::State& state) {
        flecs_world world;
        world.component<Transform>();
        world.component<Sprite>();
        world.component<RigidBody>();
        const int n = state.range(0);
        
        for (int i = 0; i < n; ++i) {
            auto e = world.entity().set<Transform>({(float)i, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f});
            int type = i % 10;
            if (type >= 4) e.set<Sprite>({(uint32_t)(i % 256), 0.f, 0.f, 1.f, 1.f, 0xFFFFFFFF, 0});
            if (type >= 7 || type < 2) e.set<RigidBody>({1.f, 0.f, 0.f, 0.f, -9.8f, 0.f, 1.f, 0.1f});
        }
        
        const float dt = 1.f / 60.f;
        auto qPhys = world.query<Transform, RigidBody>();
        auto qRender = world.query<Transform, Sprite>();
        
        for (auto _ : state) {
            qPhys.each([dt](Transform& t, RigidBody& rb) {
                rb.vy += rb.ay * dt;
                t.x += rb.vx * dt;
                t.y += rb.vy * dt;
            });
            
            float accum = 0.f;
            qRender.each([&](Transform& t, Sprite& s) { accum += t.x * (float)s.layer; });
            benchmark::DoNotOptimize(accum);
            benchmark::ClobberMemory();
        }
    }

    // Add/remove component benchmark - THIS IS WHERE ARCHETYPES HURT
    static void add_remove_component(benchmark::State& state) {
        flecs_world world;
        world.component<Position>();
        world.component<Velocity>();
        const int n = state.range(0);
        
        std::vector<flecs::entity> entities;
        entities.reserve(n);
        for (int i = 0; i < n; ++i) {
            entities.push_back(world.entity().set<Position>({(float)i, 0.f, 0.f}));
        }
        
        bool hasVelocity = false;
        for (auto _ : state) {
            if (!hasVelocity) {
                for (auto& e : entities) {
                    e.set<Velocity>({1.f, 2.f, 3.f});
                }
            } else {
                for (auto& e : entities) {
                    e.remove<Velocity>();
                }
            }
            hasVelocity = !hasVelocity;
            benchmark::ClobberMemory();
        }
    }

} // namespace flecs_r
} // namespace realistic

// Register realistic benchmarks
#define BENCH_REALISTIC_ARGS(F, ECS, FUNC) \
    F(ECS, FUNC, 1000) \
    F(ECS, FUNC, 100000) \
    F(ECS, FUNC, 1000000)

#define BENCH_REALISTIC_ONE(ECS, FUNC, ARG) \
    BENCHMARK(realistic::ECS::FUNC)->Name("realistic/" #ECS "/" #FUNC)->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(ARG)->MinTime(0.3);

#define REGISTER_REALISTIC(ecs1, ecs2, ecs3, FUNC) \
    BENCH_REALISTIC_ARGS(BENCH_REALISTIC_ONE, ecs1, FUNC) \
    BENCH_REALISTIC_ARGS(BENCH_REALISTIC_ONE, ecs2, FUNC) \
    BENCH_REALISTIC_ARGS(BENCH_REALISTIC_ONE, ecs3, FUNC)

REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, physics_integration)
REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, health_regen)
REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, ai_state_machine)
REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, sprite_batching)
REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, combat_damage)
REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, collision_broadphase)
REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, entity_churn)
REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, mixed_archetypes)
REGISTER_REALISTIC(ecss_r, entt_r, flecs_r, add_remove_component)