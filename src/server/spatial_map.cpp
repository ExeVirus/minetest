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
void SpatialMap::insert(u16 id)
{
	m_uncached.push_back(id);
}

// Invalidates upon position update
void SpatialMap::updatePosition(u16 id, v3f &oldPos, v3f& newPos)
{
	// Try to leave early if already in the same bucket:
	auto range = m_cached.equal_range(SpatialKey(newPos));
	for (auto it = range.first; it != range.second; ++it) {
		if (it->second == id) {
			return; // all good, let's get out of here
		}
	}

	// remove from cache, if present
	bool found = false;
	range = m_cached.equal_range(SpatialKey(oldPos));
	for (auto it = range.first; it != range.second; ++it) {
		if (it->second == id) {
			m_cached.erase(it);
			found = true;
			break; // Erase and leave early
		}
	}

	if(!found) {
		m_uncached.erase(std::find(m_uncached.begin(), m_uncached.end(), id));
	}
			
		// place back in uncached
	m_cached.insert(std::pair<SpatialKey,u16>(newPos, id));
}

void SpatialMap::remove(u16 id)
{
	auto found = std::find(m_uncached.begin(), m_uncached.end(), id);
	if (found != m_uncached.end()) {
		m_uncached.erase(found);
		return;
	}
	for (auto it = m_cached.begin(); it != m_cached.end();++it) {
		if(it->second == id) {
			m_cached.erase(it);
			break;
		}
	}
	// Error, this shouldn't ever be hit.
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

void SpatialMap::getRelevantObjectIds(const aabb3f &box, const std::function<void(u16 id)> &callback)
{
	if(!m_cached.empty()) {
		// when searching, we must round to maximum extent of relevant mapblock indexes
		auto absoluteRoundUp = [](f32 val) {
			//return val < 0 ? floor(val) : ceil(val);}
			s16 rounded = std::lround(val);
			s16 remainder = (rounded & 0xF) != 0; // same as (val % 8) != 0
			return (rounded >> 4) + ((rounded < 0) ? -remainder : remainder); // divide by 16 and round "up" the remainder
		};

		v3s16 min(absoluteRoundUp(box.MinEdge.X), absoluteRoundUp(box.MinEdge.Y), absoluteRoundUp(box.MinEdge.Z)),
			max(absoluteRoundUp(box.MaxEdge.X), absoluteRoundUp(box.MaxEdge.Y), absoluteRoundUp(box.MaxEdge.Z));
		
		std::vector<std::unordered_map<SpatialKey, std::vector<u16>, SpatialKeyHash>::iterator> matchingVectors;
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
	}

	// add the the rest, uncached objectIDs
	for (auto id : m_uncached) {
		callback(id);
	}
}

} // namespace server
