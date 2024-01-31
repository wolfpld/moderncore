#pragma once

#if 0

// For debug purposes only

#include <unordered_map>
#include <unordered_set>

template<typename _Key, typename _Tp,
    typename _Hash = std::hash<_Key>,
    typename _Pred = std::equal_to<_Key>,
    typename _Alloc = std::allocator<std::pair<const _Key, _Tp>>>
using unordered_flat_map = std::unordered_map<_Key, _Tp, _Hash, _Pred, _Alloc>;

template<typename _Value,
    typename _Hash = std::hash<_Value>,
    typename _Pred = std::equal_to<_Value>,
    typename _Alloc = std::allocator<_Value>>
using unordered_flat_set = std::unordered_set<_Value, _Hash, _Pred, _Alloc>;

#else

#include "contrib/robin_hood.h"

template <typename Key, typename T, typename Hash = robin_hood::hash<Key>,
          typename KeyEqual = std::equal_to<Key>, size_t MaxLoadFactor100 = 80>
using unordered_flat_map = robin_hood::unordered_flat_map<Key, T, Hash, KeyEqual>;

template <typename Key, typename Hash = robin_hood::hash<Key>, typename KeyEqual = std::equal_to<Key>,
          size_t MaxLoadFactor100 = 80>
using unordered_flat_set = robin_hood::unordered_flat_set<Key, Hash, KeyEqual, MaxLoadFactor100>;

#endif
