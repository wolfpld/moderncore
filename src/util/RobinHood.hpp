#ifndef __ROBINHOOD_HPP__
#define __ROBINHOOD_HPP__

#include "../../contrib/robin_hood.h"

template <typename Key, typename T, typename Hash = robin_hood::hash<Key>,
          typename KeyEqual = std::equal_to<Key>, size_t MaxLoadFactor100 = 80>
using unordered_flat_map = robin_hood::unordered_flat_map<Key, T, Hash, KeyEqual>;

template <typename Key, typename Hash = robin_hood::hash<Key>, typename KeyEqual = std::equal_to<Key>,
          size_t MaxLoadFactor100 = 80>
using unordered_flat_set = robin_hood::unordered_flat_set<Key, Hash, KeyEqual, MaxLoadFactor100>;

#endif
