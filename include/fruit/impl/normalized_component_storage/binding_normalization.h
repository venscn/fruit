/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FRUIT_BINDING_NORMALIZATION_H
#define FRUIT_BINDING_NORMALIZATION_H

#ifndef IN_FRUIT_CPP_FILE
// We don't want to include it in public headers to save some compile time.
#error "binding_normalization.h included in non-cpp file."
#endif

#include <fruit/impl/util/hash_helpers.h>
#include <fruit/impl/data_structures/fixed_size_allocator.h>
#include <fruit/impl/component_storage/component_storage_entry.h>
#include <fruit/impl/normalized_component_storage/normalized_component_storage.h>
#include <fruit/impl/data_structures/arena_allocator.h>

namespace fruit {
namespace impl {

/**
 * This struct contains helper functions used for binding normalization.
 * They are wrapped in a struct so that Fruit classes can easily declare to be friend
 * of all these.
 */
class BindingNormalization {
public:
  
  // Stores an element of the form (c_type_id, -> undo_info) for each binding compression that was
  // performed.
  // These are used to undo binding compression after applying it (if necessary).
  using BindingCompressionInfoMap =
      HashMapWithArenaAllocator<TypeId, NormalizedComponentStorage::CompressedBindingUndoInfo>;

  /**
   * Normalizes the toplevel entries and performs binding compression.
   * This does *not* keep track of what binding compressions were performed, so they can't be undone. When we might need
   * to undo the binding compression, use normalizeBindingsWithUndoableBindingCompression() instead.
   */
  static void normalizeBindingsWithPermanentBindingCompression(
      FixedSizeVector<ComponentStorageEntry>&& toplevel_entries,
      FixedSizeAllocator::FixedSizeAllocatorData& fixed_size_allocator_data,
      MemoryPool& memory_pool,
      const std::vector<TypeId, ArenaAllocator<TypeId>>& exposed_types,
      std::vector<ComponentStorageEntry, ArenaAllocator<ComponentStorageEntry>>& bindings_vector,
      std::unordered_map<TypeId, NormalizedMultibindingSet>& multibindings);

  /**
   * Normalizes the toplevel entries and performs binding compression, but keeps track of which compressions were
   * performed so that we can later undo some of them if needed.
   * This is more expensive than normalizeBindingsWithPermanentBindingCompression(), use that when it suffices.
   */
  static void normalizeBindingsWithUndoableBindingCompression(
      FixedSizeVector<ComponentStorageEntry>&& toplevel_entries,
      FixedSizeAllocator::FixedSizeAllocatorData& fixed_size_allocator_data,
      MemoryPool& memory_pool,
      const std::vector<TypeId, ArenaAllocator<TypeId>>& exposed_types,
      std::vector<ComponentStorageEntry, ArenaAllocator<ComponentStorageEntry>>& bindings_vector,
      std::unordered_map<TypeId, NormalizedMultibindingSet>& multibindings,
      BindingCompressionInfoMap& bindingCompressionInfoMap);

  /**
   * - FindNormalizedBinding should have a
   *   NormalizedBindingItr operator()(TypeId)
   *   that returns a NormalizedBindingItr describing whether the binding is present in a base component (if any).
   * - IsValidItr should have a
   *   bool operator()(NormalizedBindingItr)
   * - IsNormalizedBindingItrForConstructedObject should have a
   *   bool operator()(NormalizedBindingItr)
   *   (that can only be used when IsValidItr returns true)
   * - GetObjectPtr should have a
   *   ComponentStorageEntry::BindingForConstructedObject::object_ptr_t operator()(NormalizedBindingItr)
   *   (that can only be used when IsNormalizedBindingItrForConstructedObject returns true)
   * - GetCreate should have a
   *   ComponentStorageEntry::BindingForObjectToConstruct::create_t operator()(NormalizedBindingItr)
   *   (that can only be used when IsNormalizedBindingItrForConstructedObject returns false).
   */
  template <
      typename FindNormalizedBinding,
      typename IsValidItr,
      typename IsNormalizedBindingItrForConstructedObject,
      typename GetObjectPtr,
      typename GetCreate>
  static void normalizeBindingsAndAddTo(
      FixedSizeVector<ComponentStorageEntry>&& toplevel_entries,
      MemoryPool& memory_pool,
      const FixedSizeAllocator::FixedSizeAllocatorData& base_fixed_size_allocator_data,
      const std::unordered_map<TypeId, NormalizedMultibindingSet>& base_multibindings,
      const NormalizedComponentStorage::BindingCompressionInfoMap& base_binding_compression_info_map,
      FixedSizeAllocator::FixedSizeAllocatorData& fixed_size_allocator_data,
      std::vector<ComponentStorageEntry, ArenaAllocator<ComponentStorageEntry>>& new_bindings_vector,
      std::unordered_map<TypeId, NormalizedMultibindingSet>& multibindings,
      FindNormalizedBinding find_normalized_binding,
      IsValidItr is_valid_itr,
      IsNormalizedBindingItrForConstructedObject is_normalized_binding_itr_for_constructed_object,
      GetObjectPtr get_object_ptr,
      GetCreate get_create);

private:

  using multibindings_vector_elem_t = std::pair<ComponentStorageEntry, ComponentStorageEntry>;
  using multibindings_vector_t = std::vector<multibindings_vector_elem_t, ArenaAllocator<multibindings_vector_elem_t>>;

  /**
   * Adds the multibindings in multibindings_vector to the `multibindings' map.
   * Each element of multibindings_vector is a pair, where the first element is the multibinding and the second is the
   * corresponding MULTIBINDING_VECTOR_CREATOR entry.
   */
  static void addMultibindings(std::unordered_map<TypeId, NormalizedMultibindingSet>& multibindings,
                               FixedSizeAllocator::FixedSizeAllocatorData& fixed_size_allocator_data,
                               const multibindings_vector_t& multibindings_vector);

  static void printLazyComponentInstallationLoop(
      const std::vector<ComponentStorageEntry, ArenaAllocator<ComponentStorageEntry>>& entries_to_process,
      const ComponentStorageEntry& last_entry);

  /**
   * Normalizes the toplevel entries (but doesn't perform binding compression).
   * - HandleCompressedBinding should have an operator()(ComponentStorageEntry&) that will be called for each
   *   COMPRESSED_BINDING entry.
   * - HandleMultibinding should have an
   *   operator()(ComponentStorageEntry& multibinding_entry, ComponentStorageEntry& multibinding_vector_creator_entry)
   *   that will be called for each multibinding entry.
   * - FindNormalizedBinding should have a
   *   NormalizedBindingItr operator()(TypeId)
   *   that returns a NormalizedBindingItr describing whether the binding is present in a base component (if any).
   * - IsValidItr should have a
   *   bool operator()(NormalizedBindingItr)
   * - IsNormalizedBindingItrForConstructedObject should have a
   *   bool operator()(NormalizedBindingItr)
   *   (that can only be used when IsValidItr returns true)
   * - GetObjectPtr should have a
   *   ComponentStorageEntry::BindingForConstructedObject::object_ptr_t operator()(NormalizedBindingItr)
   *   (that can only be used when IsNormalizedBindingItrForConstructedObject returns true)
   * - GetCreate should have a
   *   ComponentStorageEntry::BindingForObjectToConstruct::create_t operator()(NormalizedBindingItr)
   *   (that can only be used when IsNormalizedBindingItrForConstructedObject returns false).
   */
  template <
      typename HandleCompressedBinding,
      typename HandleMultibinding,
      typename FindNormalizedBinding,
      typename IsValidItr,
      typename IsNormalizedBindingItrForConstructedObject,
      typename GetObjectPtr,
      typename GetCreate>
  static void normalizeBindings(
      FixedSizeVector<ComponentStorageEntry>&& toplevel_entries,
      FixedSizeAllocator::FixedSizeAllocatorData& fixed_size_allocator_data,
      MemoryPool& memory_pool,
      HashMapWithArenaAllocator<TypeId, ComponentStorageEntry>& binding_data_map,
      HandleCompressedBinding handle_compressed_binding,
      HandleMultibinding handle_multibinding,
      FindNormalizedBinding find_normalized_binding,
      IsValidItr is_valid_itr,
      IsNormalizedBindingItrForConstructedObject is_normalized_binding_itr_for_constructed_object,
      GetObjectPtr get_object_ptr,
      GetCreate get_create);

  struct BindingCompressionInfo {
    TypeId i_type_id;
    ComponentStorageEntry::BindingForObjectToConstruct::create_t create_i_with_compression;
  };

  /**
   * Normalizes the toplevel entries and performs binding compression.
   * - SaveCompressedBindingUndoInfo should have an operator()(TypeId, CompressedBindingUndoInfo) that will be called
   *   with (c_type_id, undo_info) for each binding compression that was applied (and that therefore might need to be
   *   undone later).
   */
  template <typename SaveCompressedBindingUndoInfo>
  static void normalizeBindingsWithBindingCompression(
      FixedSizeVector<ComponentStorageEntry>&& toplevel_entries,
      FixedSizeAllocator::FixedSizeAllocatorData& fixed_size_allocator_data,
      MemoryPool& memory_pool,
      const std::vector<TypeId, ArenaAllocator<TypeId>>& exposed_types,
      std::vector<ComponentStorageEntry, ArenaAllocator<ComponentStorageEntry>>& bindings_vector,
      std::unordered_map<TypeId, NormalizedMultibindingSet>& multibindings,
      SaveCompressedBindingUndoInfo save_compressed_binding_undo_info);

  /**
   * bindingCompressionInfoMap is an output parameter. This function will store information on all performed binding
   * compressions in that map, to allow them to be undone later, if necessary.
   * compressed_bindings_map is a map CtypeId -> (ItypeId, bindingData)
   * - SaveCompressedBindingUndoInfo should have an operator()(TypeId, CompressedBindingUndoInfo) that will be called
   *   with (c_type_id, undo_info) for each binding compression that was applied (and that therefore might need to be
   *   undone later).
   */
  template <typename SaveCompressedBindingUndoInfo>
  static std::vector<ComponentStorageEntry, ArenaAllocator<ComponentStorageEntry>> performBindingCompression(
      HashMapWithArenaAllocator<TypeId, ComponentStorageEntry>&& binding_data_map,
      HashMapWithArenaAllocator<TypeId, BindingCompressionInfo>&& compressed_bindings_map,
      MemoryPool& memory_pool,
      const multibindings_vector_t& multibindings_vector,
      const std::vector<TypeId, ArenaAllocator<TypeId>>& exposed_types,
      SaveCompressedBindingUndoInfo save_compressed_binding_undo_info);

  using LazyComponentWithNoArgs = ComponentStorageEntry::LazyComponentWithNoArgs;
  using LazyComponentWithArgs = ComponentStorageEntry::LazyComponentWithArgs;

  struct HashLazyComponentWithNoArgs {
    std::size_t operator()(const LazyComponentWithNoArgs& x) const {
      return x.hashCode();
    }
  };

  struct LazyComponentWithArgsEqualTo {
    bool operator()(const LazyComponentWithArgs& x, const LazyComponentWithArgs& y) const {
      return *x.component == *y.component;
    }
  };

  struct HashLazyComponentWithArgs {
    std::size_t operator()(const LazyComponentWithArgs& x) const {
      return x.component->hashCode();
    }
  };

  using LazyComponentWithNoArgsSet =
      HashSetWithArenaAllocator<LazyComponentWithNoArgs, HashLazyComponentWithNoArgs, std::equal_to<LazyComponentWithNoArgs>>;
  using LazyComponentWithArgsSet =
      HashSetWithArenaAllocator<LazyComponentWithArgs, HashLazyComponentWithArgs, LazyComponentWithArgsEqualTo>;

  static LazyComponentWithNoArgsSet createLazyComponentWithNoArgsSet(MemoryPool& memory_pool);
  static LazyComponentWithArgsSet createLazyComponentWithArgsSet(MemoryPool& memory_pool);

  using LazyComponentWithNoArgsReplacementMap =
      HashMapWithArenaAllocator<LazyComponentWithNoArgs, ComponentStorageEntry, HashLazyComponentWithNoArgs, std::equal_to<LazyComponentWithNoArgs>>;
  using LazyComponentWithArgsReplacementMap =
      HashMapWithArenaAllocator<LazyComponentWithArgs, ComponentStorageEntry, HashLazyComponentWithArgs, LazyComponentWithArgsEqualTo>;

  static LazyComponentWithNoArgsReplacementMap createLazyComponentWithNoArgsReplacementMap(MemoryPool& memory_pool);
  static LazyComponentWithArgsReplacementMap createLazyComponentWithArgsReplacementMap(MemoryPool& memory_pool);

  /**
   * This struct groups all data structures available during binding normalization, to avoid mentioning them in all
   * handle*Binding functions below.
   */
  template <
      typename HandleCompressedBinding,
      typename HandleMultibinding,
      typename FindNormalizedBinding,
      typename IsValidItr,
      typename IsNormalizedBindingItrForConstructedObject,
      typename GetObjectPtr,
      typename GetCreate>
  struct BindingNormalizationContext {
    FixedSizeAllocator::FixedSizeAllocatorData& fixed_size_allocator_data;
    MemoryPool& memory_pool;
    HashMapWithArenaAllocator<TypeId, ComponentStorageEntry>& binding_data_map;
    HandleCompressedBinding handle_compressed_binding;
    HandleMultibinding handle_multibinding;
    FindNormalizedBinding find_normalized_binding;
    IsValidItr is_valid_itr;
    IsNormalizedBindingItrForConstructedObject is_normalized_binding_itr_for_constructed_object;
    GetObjectPtr get_object_ptr;
    GetCreate get_create;

    // These are in reversed order (note that toplevel_entries must also be in reverse order).
    std::vector<ComponentStorageEntry, ArenaAllocator<ComponentStorageEntry>> entries_to_process;

    // These sets contain the lazy components whose expansion has already completed.
    LazyComponentWithNoArgsSet fully_expanded_components_with_no_args =
        createLazyComponentWithNoArgsSet(memory_pool);
    LazyComponentWithArgsSet fully_expanded_components_with_args =
        createLazyComponentWithArgsSet(memory_pool);

    // These sets contain the elements with kind *_END_MARKER in entries_to_process.
    // For component with args, these sets do *not* own the objects, entries_to_process does.
    LazyComponentWithNoArgsSet components_with_no_args_with_expansion_in_progress =
        createLazyComponentWithNoArgsSet(memory_pool);
    LazyComponentWithArgsSet components_with_args_with_expansion_in_progress =
        createLazyComponentWithArgsSet(memory_pool);

    // These sets contain Component replacements, as mappings componentToReplace->replacementComponent.
    LazyComponentWithNoArgsReplacementMap component_with_no_args_replacements =
        createLazyComponentWithNoArgsReplacementMap(memory_pool);
    LazyComponentWithArgsReplacementMap component_with_args_replacements =
        createLazyComponentWithArgsReplacementMap(memory_pool);

    BindingNormalizationContext(
        FixedSizeVector<ComponentStorageEntry>& toplevel_entries,
        FixedSizeAllocator::FixedSizeAllocatorData& fixed_size_allocator_data,
        MemoryPool& memory_pool,
        HashMapWithArenaAllocator<TypeId, ComponentStorageEntry>& binding_data_map,
        HandleCompressedBinding handle_compressed_binding,
        HandleMultibinding handle_multibinding,
        FindNormalizedBinding find_normalized_binding,
        IsValidItr is_valid_itr,
        IsNormalizedBindingItrForConstructedObject is_normalized_binding_itr_for_constructed_object,
        GetObjectPtr get_object_ptr,
        GetCreate get_create);

    BindingNormalizationContext(const BindingNormalizationContext&) = delete;
    BindingNormalizationContext(BindingNormalizationContext&&) = delete;

    BindingNormalizationContext& operator=(const BindingNormalizationContext&) = delete;
    BindingNormalizationContext& operator=(BindingNormalizationContext&&) = delete;

    ~BindingNormalizationContext();
  };

  template <typename... Params>
  static void handleBindingForConstructedObject(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleBindingForObjectToConstructThatNeedsAllocation(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleBindingForObjectToConstructThatNeedsNoAllocation(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleCompressedBinding(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleMultibinding(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleMultibindingVectorCreator(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleComponentWithoutArgsEndMarker(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleComponentWithArgsEndMarker(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleReplacedLazyComponentWithArgs(BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleReplacedLazyComponentWithNoArgs(BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleLazyComponentWithArgs(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void handleLazyComponentWithNoArgs(
      BindingNormalizationContext<Params...>& context);

  template <typename... Params>
  static void performComponentReplacement(
      BindingNormalizationContext<Params...>& context, const ComponentStorageEntry& replacement);

  static void printMultipleBindingsError(TypeId type);

  static void printIncompatibleComponentReplacementsError(
      const ComponentStorageEntry& replaced_component_entry,
      const ComponentStorageEntry& replacement_component_entry1,
      const ComponentStorageEntry& replacement_component_entry2);

  static void printComponentReplacementFailedBecauseTargetAlreadyExpanded(
      const ComponentStorageEntry& replaced_component_entry,
      const ComponentStorageEntry& replacement_component_entry);
};

} // namespace impl
} // namespace fruit

#endif // FRUIT_BINDING_NORMALIZATION_H
