#pragma once

#include "core/ecs/Component.h"
#include "core/ecs/Storage.h"
#include "core/ecs/World.h"

#include <array>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

// -----------------------------------------------------------------
// Query: iterate over the intersection of multiple sparse sets.
// Strategy: pick the component type with the smallest dense array
// and test every entity against the others via HasEntity.
// -----------------------------------------------------------------
template <class... Cs>
class Query {
		static_assert((... && std::is_base_of_v<ComponentBase, Cs>), "All types in Query must derive from ComponentBase");
		static_assert(sizeof...(Cs) > 0, "Query must contain at least one component type");

		World *world_;

public:
		explicit Query(World &w) : world_(&w) {}

		template <class F>
		void ForEach(F &&fn) {
				if constexpr (sizeof...(Cs) == 1) {
						ForEachSingle<Cs...>(std::forward<F>(fn));
				} else {
						ForEachMulti(std::forward<F>(fn));
				}
		}

private:
		// Single component: simple iteration.
		template <class C, class F>
		void ForEachSingle(F &&fn) {
				auto *storage = world_->template GetStorage<C>();
				size_t sz = storage->Size();
				for (size_t i = 0; i < sz; ++i) {
						EntityIndex eidx = storage->EntityAtDense(i);
						EntityGeneration gen = world_->GetGeneration(eidx);
						if (gen == INVALID_GENERATION) {
								continue;
						}
						Entity e{ eidx, gen };
						if (!world_->IsAlive(e)) {
								continue;
						}
						fn(e, storage->AtDense(i));
				}
		}

		// Helper to get a specific component pointer by type index in the pack.
		template <std::size_t I, class... Ts>
		struct TypeAt;
		template <std::size_t I, class T, class... Ts>
		struct TypeAt<I, T, Ts...> {
				using type = typename TypeAt<I - 1, Ts...>::type;
		};
		template <class T, class... Ts>
		struct TypeAt<0, T, Ts...> {
				using type = T;
		};

		// Multi-component: find smallest set, iterate it, check intersection.
		template <class F>
		void ForEachMulti(F &&fn) {
				constexpr std::size_t N = sizeof...(Cs);

				// Array of storages and sizes
				IComponentStorage *const storages[N] = { world_->template GetStorage<Cs>()... };
				const std::size_t sizes[N] = { world_->template GetStorage<Cs>()->Size()... };

				// Find smallest storage index
				std::size_t leadIdx = 0;
				for (std::size_t i = 1; i < N; ++i) {
						if (sizes[i] < sizes[leadIdx]) {
								leadIdx = i;
						}
				}

				IComponentStorage *leadStorage = storages[leadIdx];
				if (!leadStorage || leadStorage->Size() == 0) {
						return;
				}

				size_t leadSize = leadStorage->Size();
				for (size_t i = 0; i < leadSize; ++i) {
						EntityIndex eidx = leadStorage->EntityAtDense(i);
						EntityGeneration gen = world_->GetGeneration(eidx);
						if (gen == INVALID_GENERATION) {
								continue;
						}
						Entity e{ eidx, gen };
						if (!world_->IsAlive(e)) {
								continue;
						}

						// Check all component types are present.
						bool allPresent = (world_->template HasComponent<Cs>(e) && ...);
						if (!allPresent) {
								continue;
						}

						// Retrieve all components and call fn.
						fn(e, *world_->template GetComponent<Cs>(e)...);
				}
		}
};
