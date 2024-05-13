/*
Minetest
Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "spatial_map.h"
#include <algorithm>

namespace server
{

// all inserted entires go into the uncached vector
void SpatialMap::insert(u16 id, const v3f &pos)
{
	m_cached.insert({SpatialKey(pos), id});
}

// Invalidates upon position update
void SpatialMap::updatePosition(u16 id, const v3f &oldPos, const v3f &newPos)
{
	// Try to leave early if already in the same bucket:
	auto range = m_cached.equal_range(SpatialKey(newPos));
	for (auto it = range.first; it != range.second; ++it) {
		if (it->second == id) {
			return; // all good, let's get out of here
		}
	}

	// remove from old cache position
	range = m_cached.equal_range(SpatialKey(oldPos));
	for (auto it = range.first; it != range.second; ++it) {
		if (it->second == id) {
			m_cached.erase(it);
			break; // Erase and leave early
		}
	}

	// place in new bucket
	insert(id, newPos);
}

void SpatialMap::remove(u16 id, const v3f &pos)
{
	SpatialKey key(pos);
	if(m_cached.find(key) != m_cached.end()) {
		auto range = m_cached.equal_range(key);
		for (auto it = range.first; it != range.second; ++it) {
			if (it->second == id) {
				m_cached.erase(it);
				return; // Erase and leave early
			}
		}
	}
	remove(id); // should never be hit
}

void SpatialMap::remove(u16 id)
{
	for (auto it = m_cached.begin(); it != m_cached.end(); ++it) {
		if (it->second == id) {
			m_cached.erase(it);
			return; // Erase and leave early
		}
	}
}

void SpatialMap::removeAll()
{
	m_cached.clear();
}

void SpatialMap::removeMapblock(const v3f &mapblockOrigin)
{
	m_cached.erase(SpatialKey(mapblockOrigin));
}

void SpatialMap::getRelevantObjectIds(const aabb3f &box, const std::function<void(u16 id)> &callback)
{
	if(!m_cached.empty()) {
		// when searching, we must round to maximum extent of relevant mapblock indexes
		auto absoluteRoundUp = [](f32 val) {
			//return val < 0 ? floor(val) : ceil(val);}
			s16 rounded = std::lround(val);
			s16 remainder = (rounded & 0xF) != 0; // same as (val % 16) != 0
			return (rounded >> 4) + ((rounded < 0) ? -remainder : remainder); // divide by 16 and round "up" the remainder
		};

		v3s16 min(absoluteRoundUp(box.MinEdge.X), absoluteRoundUp(box.MinEdge.Y), absoluteRoundUp(box.MinEdge.Z)),
			max(absoluteRoundUp(box.MaxEdge.X), absoluteRoundUp(box.MaxEdge.Y), absoluteRoundUp(box.MaxEdge.Z));
		
		// We should only iterate using this spatial map when there are at least 1 objects per mapblocks to check.
		// Otherwise, might as well just iterate.

		v3s16 diff = max - min;
		uint64_t number_of_mapblocks_to_check = std::abs(diff.X) * std::abs(diff.Y) * std::abs(diff.Z);
		if(number_of_mapblocks_to_check <= m_cached.size()) { // might be worth it
			for (s16 x = min.X; x < max.X;x++) {
				for (s16 y = min.Y; y < max.Y;y++) {
					for (s16 z = min.Z; z < max.Z;z++) {
						SpatialKey key(x,y,z, false);
						if (m_cached.find(key) != m_cached.end()) {
							auto range = m_cached.equal_range(key);
							for (auto &it = range.first; it != range.second; ++it) {
								callback(it->second);
							}
						}
					}
				}
			}
		} else { // let's just iterate, it'll be faster
			for (auto it = m_cached.begin(); it != m_cached.end(); ++it) {
				callback(it->second);
			}
		}
	}
}

} // namespace server
