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

namespace server
{

// all inserted entires go into the uncached vector
void SpatialMap::insert(u16 id)
{
	m_uncached.push_back(id);
}

// Invalidates upon position update
void SpatialMap::invalidate(u16 id, v3f &pos)
{
	// remove from cache, if present
	bool found = false;
	SpatialKey key(pos);
	auto range = m_cached.equal_range(key);
	for (auto it = range.first; it != range.second; ++it) {
		if (it->second == id) {
			m_cached.erase(it);
			found = true;
			break; // Erase and leave early
		}
	}

	if(found) {
		// place back in uncached
		insert(id);
	}
}

void SpatialMap::remove(u16 id, v3f pos)
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
	};

	auto it = std::find(m_uncached.begin(), m_uncached.end(), id);
	if (it != m_uncached.end()) {
		m_uncached.erase(it);
		return;
	} else {
		// Error, this shouldn't ever be hit.
	}
}

// Only when at least 64 uncached objects or 10% uncached overall
void SpatialMap::cacheUpdate(const std::function<v3f(u16)> &getPos)
{
	if(m_uncached.size() >= 64 || (m_uncached.size() >= 64 && m_uncached.size() * 10 > m_cached.size())) {
		for(u16& entry : m_uncached) {
			m_cached.insert(std::pair<SpatialKey,u16>(getPos(entry), entry));
		}
		m_uncached.clear();
	}
}

void SpatialMap::getRelevantObjectIds(const aabb3f &box, std::vector<u16> &relevant_objs)
{
	if(!m_cached.empty()) {
		// when searching, we must round to maximum extent of relevant mapblock indexes
		auto shrinkRnd = [](f32 val) {
			//return val < 0 ? floor(val) : ceil(val);}
			s16 rounded = std::lround(val);
			s16 remainder = (rounded & 0xF) != 0; // same as (val % 16) != 0
			return (rounded >> 4) + ((rounded < 0) ? -remainder : remainder);
		};

		v3s16 min(shrinkRnd(box.MinEdge.X), shrinkRnd(box.MinEdge.Y), shrinkRnd(box.MinEdge.Z)),
			max(shrinkRnd(box.MaxEdge.X), shrinkRnd(box.MaxEdge.Y), shrinkRnd(box.MaxEdge.Z));
		
		for (int x = box.MinEdge.X; x < box.MaxEdge.X;x++) {
			for (int y = box.MinEdge.Y; y < box.MaxEdge.Y;y++) {
				for (int z = box.MinEdge.Z; z < box.MaxEdge.Z;z++) {
					SpatialKey key(x,y,z);
					if (m_cached.find(key) != m_cached.end()) {
						auto range = m_cached.equal_range(key);
						for (auto &it = range.first; it != range.second; ++it) {
							relevant_objs.push_back(it->second);
						}
					}
				}
			}
		}
	}

	// add the the rest, uncached objectIDs
	relevant_objs.insert(relevant_objs.end(), m_uncached.begin(), m_uncached.end());
}

} // namespace server
