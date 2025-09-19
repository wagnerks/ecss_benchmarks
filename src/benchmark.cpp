#include <benchmark/benchmark.h>
#include <entt/entt.hpp>
#include <vector>
#include <cstdint>
#include <ecss/Registry.h>
#include <vector>
#include <cstdint>

struct Position { float x, y, z; };

namespace ecss
{
    // Insert int component per entity (original benchmark kept for compatibility)
    static void insert(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            reg.reserve<int>(static_cast<size_t>(state.range(0)));
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<int>(e, 42);
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

    // Add int component (separate name for clarity)
    static void add_int_component(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            reg.reserve<int>(static_cast<size_t>(state.range(0)));
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<int>(e, i);
            }
        }
    }

    // Add struct component
    static void add_struct_component(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            reg.reserve<Position>(static_cast<size_t>(state.range(0)));
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
            }
        }
    }

    // Grouped array registration then insert two components
    static void grouped_insert(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        for (auto _ : state) {
            Reg reg;
            reg.registerArray<int, Position>(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<int>(e, 7);
                reg.addComponent<Position>(e, Position{ 1.f,2.f,3.f });
            }
        }
    }

    // hasComponent over existing components
    static void has_component(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        using ecss::EntityId;
        Reg reg;
        std::vector<EntityId> ids;
        ids.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<int>(e, 1);
            ids.push_back(e);
        }
        for (auto _ : state) {
            size_t count = 0;
            for (auto id : ids) {
                if (reg.hasComponent<int>(id)) ++count;
            }
            benchmark::DoNotOptimize(count);
        }
    }

    // Batch destroy entities (measures destroy cost only using Pause/Resume)
    static void destroy_entities(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        using ecss::EntityId;
        for (auto _ : state) {
            state.PauseTiming();
            Reg reg;
            std::vector<EntityId> ids; ids.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.takeEntity();
                reg.addComponent<int>(e, 3);
                reg.addComponent<Position>(e, Position{ 0,0,0 });
                ids.push_back(e);
            }
            state.ResumeTiming();
            reg.destroyEntities(ids);
            benchmark::ClobberMemory();
        }
    }

    // ================= Iteration Benchmarks =====================
    // Iterate a single component array (baseline)
    static void iter_single_component(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        Reg reg;
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<int>(e, i);
        }
        for (auto _ : state) {
            size_t sum = 0;
            for (auto [eid, val] : reg.view<int>()) {
                sum += (size_t)*val;
            }
            benchmark::DoNotOptimize(sum);
        }
    }

    // Iterate multiple components that are grouped into a single sectors array
    static void iter_grouped_multi(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        Reg reg;
        reg.registerArray<int, Position>();
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<int>(e, i);
            reg.addComponent<Position>(e, Position{ (float)i, (float)i * 2.f, (float)i * 3.f });
        }
        for (auto _ : state) {
            float accum = 0.f;
            for (auto [eid, iptr, pptr] : reg.view<int, Position>()) {
                accum += *iptr;
                accum += pptr->x + pptr->y + pptr->z;
            }
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iterate multiple components that live in separate sectors arrays
    static void iter_separate_multi(benchmark::State& state) {
        using Reg = ecss::Registry<false>;
        Reg reg; // no grouping call => each component gets its own array lazily
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.takeEntity();
            reg.addComponent<int>(e, i);
            reg.addComponent<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
        }
        for (auto _ : state) {
            float accum = 0.f;
            for (auto [eid, iptr, pptr] : reg.view<int, Position>()) {
                // Both should be non-null since we added both components
                accum += *iptr + pptr->x + pptr->y + pptr->z;
            }
            benchmark::DoNotOptimize(accum);
        }
    }
}

namespace entt
{
    using big_registry = entt::registry;
    // Insert int component per entity (baseline insertion with constant value)
    static void insert(benchmark::State& state) {
        for (auto _ : state) {
            entt::big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<Position>(e, (float)42);
            }
        }
    }

    // Create entities only (no components)
    static void create_entities(benchmark::State& state) {
        for (auto _ : state) {
            entt::big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                benchmark::DoNotOptimize(reg.create());
            }
        }
    }

    // Add int component with varying value
    static void add_int_component(benchmark::State& state) {
        for (auto _ : state) {
            entt::big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<int>(e, i);
            }
        }
    }

    // Add struct component (Position)
    static void add_struct_component(benchmark::State& state) {
        for (auto _ : state) {
            entt::big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
            }
        }
    }

    // Multi-component insert (int + Position)
    static void grouped_insert(benchmark::State& state) {
        for (auto _ : state) {
            entt::big_registry reg;
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<int>(e, 7);
                reg.emplace<Position>(e, Position{ 1.f, 2.f, 3.f });
            }
        }
    }

    // hasComponent equivalent: all_of<int>
    static void has_component(benchmark::State& state) {
        entt::big_registry reg;
        std::vector<entt::big_registry::entity_type> ids; ids.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.create();
            reg.emplace<int>(e, 1);
            ids.push_back(e);
        }
        for (auto _ : state) {
            size_t count = 0;
            for (auto e : ids) {
                if (reg.all_of<int>(e)) ++count;
            }
            benchmark::DoNotOptimize(count);
        }
    }

    // Batch destroy entities (pause to exclude setup)
    static void destroy_entities(benchmark::State& state) {
        for (auto _ : state) {
            state.PauseTiming();
            entt::big_registry reg;
            std::vector<entt::big_registry::entity_type> ids; ids.reserve(state.range(0));
            for (int i = 0; i < state.range(0); ++i) {
                auto e = reg.create();
                reg.emplace<int>(e, 3);
                reg.emplace<Position>(e, Position{ 0,0,0 });
                ids.push_back(e);
            }
            state.ResumeTiming();
            reg.destroy(ids.begin(), ids.end());
            benchmark::ClobberMemory();
        }
    }
    // Iteration single component
    static void iter_single_component(benchmark::State& state) {
        entt::big_registry reg;
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.create();
            reg.emplace<Position>(e, (float)i);
        }
        auto view = reg.view<Position>();
        for (auto _ : state) {
            size_t sum = 0;
            for (auto entity : view) {
                auto& val = view.get<Position>(entity);
                sum += (size_t)val.x;
            }
            benchmark::DoNotOptimize(sum);
        }
    }

    // Iteration multi component (grouped analog - same for entt)
    static void iter_grouped_multi(benchmark::State& state) {
        entt::big_registry reg;
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.create();
            reg.emplace<int>(e, i);
            reg.emplace<Position>(e, Position{ (float)i, (float)i * 2.f, (float)i * 3.f });
        }
        auto view = reg.view<int, Position>();
        for (auto _ : state) {
            float accum = 0.f;
            for (auto entity : view) {
                auto [ival, pos] = view.get<int, Position>(entity);
                accum += ival + pos.x + pos.y + pos.z;
            }
            benchmark::DoNotOptimize(accum);
        }
    }

    // Iteration multi component (separate analog - identical in entt)
    static void iter_separate_multi(benchmark::State& state) {
        entt::big_registry reg;
        for (int i = 0; i < state.range(0); ++i) {
            auto e = reg.create();
            reg.emplace<int>(e, i);
            reg.emplace<Position>(e, Position{ (float)i, (float)i + 1.f, (float)i + 2.f });
        }
        auto view = reg.view<int, Position>();
        for (auto _ : state) {
            float accum = 0.f;
            for (auto entity : view) {
                auto [ival, pos] = view.get<int, Position>(entity);
                accum += ival + pos.x + pos.y + pos.z;
            }
            benchmark::DoNotOptimize(accum);
        }
    }
}

#define TO_FUNC_NAME(funcName, ecs) #ecs "....................." #funcName

#define REGISTER_BENCHMARK(ecs0, ecs1, funcName) \
BENCHMARK(ecs0::funcName)->Name(TO_FUNC_NAME(funcName, ecs0))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(1024);\
BENCHMARK(ecs1::funcName)->Name(TO_FUNC_NAME(funcName, ecs1))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(1024);\
BENCHMARK(ecs0::funcName)->Name(TO_FUNC_NAME(funcName, ecs0))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(4096);\
BENCHMARK(ecs1::funcName)->Name(TO_FUNC_NAME(funcName, ecs1))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(4096);\
BENCHMARK(ecs0::funcName)->Name(TO_FUNC_NAME(funcName, ecs0))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(32768);\
BENCHMARK(ecs1::funcName)->Name(TO_FUNC_NAME(funcName, ecs1))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(32768);\
BENCHMARK(ecs0::funcName)->Name(TO_FUNC_NAME(funcName, ecs0))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(262144);\
BENCHMARK(ecs1::funcName)->Name(TO_FUNC_NAME(funcName, ecs1))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(262144);\
BENCHMARK(ecs0::funcName)->Name(TO_FUNC_NAME(funcName, ecs0))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(1'000'000);\
BENCHMARK(ecs1::funcName)->Name(TO_FUNC_NAME(funcName, ecs1))->Unit(benchmark::TimeUnit::kMicrosecond)->Arg(1'000'000);\

REGISTER_BENCHMARK(ecss, entt, insert)
REGISTER_BENCHMARK(ecss, entt, create_entities)
REGISTER_BENCHMARK(ecss, entt, add_int_component)
REGISTER_BENCHMARK(ecss, entt, add_struct_component)
REGISTER_BENCHMARK(ecss, entt, grouped_insert)
REGISTER_BENCHMARK(ecss, entt, has_component)
REGISTER_BENCHMARK(ecss, entt, destroy_entities)
REGISTER_BENCHMARK(ecss, entt, iter_single_component)
REGISTER_BENCHMARK(ecss, entt, iter_grouped_multi)
REGISTER_BENCHMARK(ecss, entt, iter_separate_multi)

#if ECSS_SINGLE_BENCHS
// ECSS benchmarks with 100 million entities (only ECSS to avoid too many runs)
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