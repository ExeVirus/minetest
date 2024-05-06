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

#pragma once

#include <functional>
#include <vector>
#include "serveractiveobject.h"
#include "activeobjectmgr.h"

namespace server
{
class SpatialMap
{
public:
	// all inserted entires go into the uncached vector
	void insert(ServerActiveObject* obj);
	
	// Invalidates upon position update or removal
	void invalidate(ServerActiveObject* obj);

	// On active_object removal, remove.
	void remove(ServerActiveObject* obj);

	// Only when at least 64 uncached objects or 10% uncached overall
	void cacheUpdate(ActiveObjectMgr& mgr);

	// Use the same basic algorithm for both area and radius lookups
	void getRelevantObjectIds(const aabb3f &box, std::vector<u16> &relevant_objs);

protected:
	typedef struct SpatialKey{
		u16 padding{0};
		s16 x;
		s16 y;
		s16 z;

		SpatialKey(s16 _x, s16 _y, s16 _z) {
			x = _x >> 4;
			y = _y >> 4;
			z = _z >> 4;
		}
		SpatialKey(v3f _pos) : SpatialKey(_pos.X, _pos.Y, _pos.Z){}
	} SpatialKey;

	std::unordered_multimap<SpatialKey, u16> m_cached;
	std::vector<u16> m_uncached;
};
} // namespace server
