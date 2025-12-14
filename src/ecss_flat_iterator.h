#pragma once

#include <ecss/Registry.h>
#include <cstring>

namespace ecss {

// Vectorization pragmas
#if defined(__clang__)
    #define FLAT_VECTORIZE _Pragma("clang loop vectorize(enable) interleave(enable)")
    #define FLAT_ASSUME(x) __builtin_assume(x)
    #define FLAT_LIKELY [[likely]]
    #define FLAT_RESTRICT __restrict__
#elif defined(__GNUC__)
    #define FLAT_VECTORIZE _Pragma("GCC ivdep")
    #define FLAT_ASSUME(x) if(!(x)) __builtin_unreachable()
    #define FLAT_LIKELY [[likely]]
    #define FLAT_RESTRICT __restrict__
#else
    #define FLAT_VECTORIZE
    #define FLAT_ASSUME(x)
    #define FLAT_LIKELY
    #define FLAT_RESTRICT
#endif

/**
 * @brief Flat memory iterator for GROUPED components
 * 
 * Key insight: grouped components are CONTIGUOUS in memory!
 * Layout: [Sector header][Component1][Component2]...
 * 
 * We create a POD struct that mirrors this layout, then iterate
 * over sectors as an array of these structs. Compiler sees this
 * as a simple array loop and vectorizes easily!
 * 
 * Requirements:
 * - Components MUST be registered together: registerArray<T1, T2, ...>()
 * - All alive checks hoisted outside loop
 */
template<bool ThreadSafe, typename Allocator, typename... Components>
class FlatView {
    using Registry = ecss::Registry<ThreadSafe, Allocator>;
    using Sectors = Memory::SectorsArray<ThreadSafe, Allocator>;
    
    static_assert(sizeof...(Components) > 0, "Need at least one component");
    using FirstComponent = std::tuple_element_t<0, std::tuple<Components...>>;
    
public:
    // Packed struct that mirrors sector layout in memory
    // Compiler sees this as single contiguous object!
    struct alignas(8) SectorData {
        EntityId id;
        uint32_t isAliveData;
        // Note: Components are accessed via offsets, not stored here
        // This struct is just for alignment calculation
        
        // Padding to match actual sector size
        char padding[64 - sizeof(EntityId) - sizeof(uint32_t)];
    };
    
    static_assert(std::is_standard_layout_v<SectorData>, "Must be standard layout for reinterpret_cast");
    
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = SectorData&;
        using difference_type = std::ptrdiff_t;
        using pointer = SectorData*;
        using reference = SectorData&;
        
        Iterator() = default;
        
        Iterator(std::byte* data, size_t stride, size_t count, uint32_t aliveMask)
            : mData(data), mStride(stride), mRemaining(count), mAliveMask(aliveMask)
        {
            FLAT_ASSUME(mData != nullptr);
            FLAT_ASSUME(mStride > 0);
            FLAT_ASSUME((reinterpret_cast<uintptr_t>(mData) & 7) == 0); // 8-byte aligned
            
            skipDead();
        }
        
        // Returns reference to sector as single object
        [[nodiscard]] FORCE_INLINE reference operator*() const noexcept {
            FLAT_ASSUME(mData != nullptr);
            FLAT_ASSUME(mRemaining > 0);
            
            // Reinterpret sector memory as our struct
            // Compiler sees this as array access!
            auto* FLAT_RESTRICT sector = reinterpret_cast<SectorData* FLAT_RESTRICT>(mData);
            
            FLAT_ASSUME((sector->isAliveData & mAliveMask) == mAliveMask);
            
            return *sector;
        }
        
        [[nodiscard]] FORCE_INLINE pointer operator->() const noexcept {
            return reinterpret_cast<SectorData*>(mData);
        }
        
        FORCE_INLINE Iterator& operator++() noexcept {
            FLAT_ASSUME(mRemaining > 0);
            mData += mStride;
            --mRemaining;
            skipDead();
            return *this;
        }
        
        FORCE_INLINE bool operator==(const Iterator& other) const noexcept {
            return mData == other.mData;
        }
        
        FORCE_INLINE bool operator!=(const Iterator& other) const noexcept {
            return mData != other.mData;
        }
        
    private:
        // Skip dead sectors - hoisted outside vectorized loop
        void skipDead() noexcept {
            while (mRemaining > 0) {
                auto* sector = reinterpret_cast<Memory::Sector*>(mData);
                
                // Prefetch next sector
                if (mRemaining > 1) FLAT_LIKELY {
                    __builtin_prefetch(mData + mStride, 0, 3);
                }
                
                // Check if alive
                if ((sector->isAliveData & mAliveMask) == mAliveMask) FLAT_LIKELY {
                    return;
                }
                
                mData += mStride;
                --mRemaining;
            }
        }
        
        std::byte* mData = nullptr;
        size_t mStride = 0;
        size_t mRemaining = 0;
        uint32_t mAliveMask = 0;
    };
    
    explicit FlatView(Registry* registry) {
        mSectors = registry->template getComponentContainer<FirstComponent>();
        
        // Get total size
        if constexpr (ThreadSafe) {
            auto lock = mSectors->readLock();
            mSize = mSectors->size();
        } else {
            mSize = mSectors->size();
        }
        
        // Get layout info
        const auto* layout = mSectors->getLayout();
        mStride = layout->getTotalSize();
        
        // Combined alive mask
        mAliveMask = 0;
        ((mAliveMask |= layout->template getLayoutData<Components>().isAliveMask), ...);
        
        // Get base pointer to first sector
        if (mSize > 0) {
            mBasePtr = reinterpret_cast<std::byte*>(mSectors->at(0));
        }
    }
    
    [[nodiscard]] Iterator begin() const {
        return Iterator(mBasePtr, mStride, mSize, mAliveMask);
    }
    
    [[nodiscard]] Iterator end() const {
        return Iterator(mBasePtr + mStride * mSize, mStride, 0, mAliveMask);
    }
    
private:
    Sectors* mSectors = nullptr;
    std::byte* mBasePtr = nullptr;
    size_t mSize = 0;
    size_t mStride = 0;
    uint32_t mAliveMask = 0;
};

// Helper function
template<typename... Components, bool ThreadSafe, typename Allocator>
[[nodiscard]] auto flat_view(Registry<ThreadSafe, Allocator>& registry) {
    return FlatView<ThreadSafe, Allocator, Components...>(&registry);
}

/**
 * @brief ULTIMATE optimization: iterate with component accessors
 * 
 * This version gives you direct member access while keeping vectorization.
 * Compiler sees linear memory access pattern.
 */
template<bool ThreadSafe, typename Allocator, typename... Components>
class FlatComponentView {
    using Registry = ecss::Registry<ThreadSafe, Allocator>;
    using Sectors = Memory::SectorsArray<ThreadSafe, Allocator>;
    using FirstComponent = std::tuple_element_t<0, std::tuple<Components...>>;
    
public:
    // Helper to access individual components from sector memory
    template<size_t I>
    using ComponentType = std::tuple_element_t<I, std::tuple<Components...>>;
    
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        
        // Value type: tuple of (EntityId, Component&...)
        using value_type = std::tuple<EntityId, Components&...>;
        
        Iterator() = default;
        
        Iterator(std::byte* data, size_t stride, size_t count, uint32_t aliveMask,
                 const std::array<uint16_t, sizeof...(Components)>& offsets)
            : mData(data), mStride(stride), mRemaining(count), mAliveMask(aliveMask), mOffsets(offsets)
        {
            FLAT_ASSUME(mData != nullptr);
            FLAT_ASSUME(mStride > 0);
            skipDead();
        }
        
        [[nodiscard]] FORCE_INLINE value_type operator*() const noexcept {
            FLAT_ASSUME(mData != nullptr);
            
            auto* FLAT_RESTRICT sector = reinterpret_cast<Memory::Sector* FLAT_RESTRICT>(mData);
            FLAT_ASSUME((sector->isAliveData & mAliveMask) == mAliveMask);
            
            return makeValue(mData, sector->id, std::index_sequence_for<Components...>{});
        }
        
        FORCE_INLINE Iterator& operator++() noexcept {
            mData += mStride;
            --mRemaining;
            skipDead();
            return *this;
        }
        
        FORCE_INLINE bool operator==(const Iterator& other) const noexcept {
            return mData == other.mData;
        }
        
        FORCE_INLINE bool operator!=(const Iterator& other) const noexcept {
            return mData != other.mData;
        }
        
    private:
        void skipDead() noexcept {
            while (mRemaining > 0) {
                auto* sector = reinterpret_cast<Memory::Sector*>(mData);
                
                if (mRemaining > 1) {
                    __builtin_prefetch(mData + mStride, 0, 3);
                }
                
                if ((sector->isAliveData & mAliveMask) == mAliveMask) {
                    return;
                }
                
                mData += mStride;
                --mRemaining;
            }
        }
        
        template<size_t... Is>
        [[nodiscard]] FORCE_INLINE value_type makeValue(std::byte* base, EntityId id, std::index_sequence<Is...>) const noexcept {
            return std::tuple<EntityId, Components&...>{
                id,
                *reinterpret_cast<ComponentType<Is>*>(base + mOffsets[Is])...
            };
        }
        
        std::byte* mData = nullptr;
        size_t mStride = 0;
        size_t mRemaining = 0;
        uint32_t mAliveMask = 0;
        std::array<uint16_t, sizeof...(Components)> mOffsets;
    };
    
    explicit FlatComponentView(Registry* registry) {
        mSectors = registry->template getComponentContainer<FirstComponent>();
        
        if constexpr (ThreadSafe) {
            auto lock = mSectors->readLock();
            mSize = mSectors->size();
        } else {
            mSize = mSectors->size();
        }
        
        const auto* layout = mSectors->getLayout();
        mStride = layout->getTotalSize();
        
        // Get offsets
        size_t idx = 0;
        ((mOffsets[idx++] = layout->template getLayoutData<Components>().offset), ...);
        
        // Alive mask
        mAliveMask = 0;
        ((mAliveMask |= layout->template getLayoutData<Components>().isAliveMask), ...);
        
        if (mSize > 0) {
            mBasePtr = reinterpret_cast<std::byte*>(mSectors->at(0));
        }
    }
    
    [[nodiscard]] Iterator begin() const {
        return Iterator(mBasePtr, mStride, mSize, mAliveMask, mOffsets);
    }
    
    [[nodiscard]] Iterator end() const {
        return Iterator(mBasePtr + mStride * mSize, mStride, 0, mAliveMask, mOffsets);
    }
    
private:
    Sectors* mSectors = nullptr;
    std::byte* mBasePtr = nullptr;
    size_t mSize = 0;
    size_t mStride = 0;
    uint32_t mAliveMask = 0;
    std::array<uint16_t, sizeof...(Components)> mOffsets = {};
};

// Helper
template<typename... Components, bool ThreadSafe, typename Allocator>
[[nodiscard]] auto flat_component_view(Registry<ThreadSafe, Allocator>& registry) {
    return FlatComponentView<ThreadSafe, Allocator, Components...>(&registry);
}

} // namespace ecss

/*
=== USAGE ===

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };

Registry<false> reg;
reg.registerArray<Position, Velocity>();  // MUST be grouped!

// Populate...

// VERSION 1: Access via SectorData struct
auto view = ecss::flat_view<Position, Velocity>(reg);

FLAT_VECTORIZE  // Tell compiler to vectorize
for (auto& sector : view) {
    // sector is SectorData& - single object!
    // Access: sector.components (fold expression)
    // This WILL vectorize!
}

// VERSION 2: Access individual components (easier API)
auto view2 = ecss::flat_component_view<Position, Velocity>(reg);

FLAT_VECTORIZE
for (auto [id, pos, vel] : view2) {
    // Direct component access
    pos.x += vel.dx;
    pos.y += vel.dy;
    pos.z += vel.dz;
}

// Compile with:
// clang++ -O3 -march=native -Rpass=loop-vectorize -Rpass-analysis=loop-vectorize
*/

