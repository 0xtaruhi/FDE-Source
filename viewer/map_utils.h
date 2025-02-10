#ifndef FDU_MAP_UTILS_H
#define FDU_MAP_UTILS_H

#include <type_traits>

template <typename Map,
	typename K = typename Map::key_type,
	typename V = typename Map::mapped_type,
	typename P = std::add_pointer_t<std::remove_pointer_t<V>>
>
P find_ptr(Map & m, const K & k) {
	auto it = m.find(k);
	if (it == m.end())
		return nullptr;
	if constexpr (std::is_pointer_v<V>)
		return it->second;
	else
		return &it->second;
}

#endif