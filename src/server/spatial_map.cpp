/*
Minetest
Copyright (C) 2024, ExeVirus <nodecastmt@gmail.com>

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
	if(!m_iterators_stopping_insertion_and_deletion) {
		m_cached.insert({SpatialKey(pos), id});
	} else {
		m_pending_inserts.insert(SpatialKey(pos,id));
	}
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

	remove(id, oldPos); // remove from old cache position
	insert(id, newPos); // reinsert
}

void SpatialMap::remove(u16 id, const v3f &pos)
{
	if(!m_iterators_stopping_insertion_and_deletion) {
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
	} else {
		m_pending_deletes.insert(SpatialKey(pos, id));
		return;
	}
	remove(id); // should never be hit
}

void SpatialMap::remove(u16 id)
{
	if(!m_iterators_stopping_insertion_and_deletion) {
		for (auto it = m_cached.begin(); it != m_cached.end(); ++it) {
			if (it->second == id) {
				m_cached.erase(it);
				break; // Erase and leave early
			}
		}
	} else {
		m_pending_deletes.insert(SpatialKey(v3f(), id));
	}
}

void SpatialMap::removeAll()
{
	if(!m_iterators_stopping_insertion_and_deletion) {
		m_cached.clear();
	} else {
		m_remove_all = true;
	}
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
							m_iterators_stopping_insertion_and_deletion++;
							auto range = m_cached.equal_range(key);
							for (auto &it = range.first; it != range.second; ++it) {
								callback(it->second);
							}
							m_iterators_stopping_insertion_and_deletion--;
							handleInsertsAndDeletes();
						}
					}
				}
			}
		} else { // let's just iterate, it'll be faster
			m_iterators_stopping_insertion_and_deletion++;
			for (auto it = m_cached.begin(); it != m_cached.end(); ++it) {
				callback(it->second);
			}
			m_iterators_stopping_insertion_and_deletion--;
			handleInsertsAndDeletes();
		}
	}
}


void SpatialMap::getObjectsIdsInRadius(const v3f &pos, float radius, const std::function<void(u16 id)> &needsCheckedCallback,
		const std::function<void(u16 id)>  &guarunteedCallback)
{
	if(!m_cached.empty()) {
		float r2 = radius * radius;

		// when searching, we must round to maximum extent of relevant mapblock indexes
		auto absoluteRoundUp = [](f32 val) -> s16 {
			//return val < 0 ? floor(val) : ceil(val);}
			s16 rounded = std::lround(val);
			s16 remainder = (rounded & 0xF) != 0; // same as (val % 16) != 0
			return (rounded >> 4) + ((rounded < 0) ? -remainder : remainder); // divide by 16 and round "up" the remainder
		};

		v3s16 min(absoluteRoundUp(pos.X-radius), absoluteRoundUp(pos.Y-radius), absoluteRoundUp(pos.Z-radius)),
			max(absoluteRoundUp(pos.X+radius), absoluteRoundUp(pos.Y+radius), absoluteRoundUp(pos.Z+radius));
		
		// We should only iterate using this spatial map when there are at least 1 objects per mapblocks to check.
		// Otherwise, might as well just iterate.

		v3s16 diff = max - min;
		uint64_t number_of_mapblocks_to_check = std::abs(diff.X) * std::abs(diff.Y) * std::abs(diff.Z);
		if(number_of_mapblocks_to_check <= m_cached.size() + 100) { // might be worth it
			for (s16 x = min.X; x < max.X;x++) {
				// search only mapblocks inside/intersecting the sphere, only matters for radius's larger than ~60 or so
				s16 startY = min.Y;
				s16 startZ = min.Z;
				s16 endY   = max.Y;
				s16 endZ   = max.Z;
				// grab a unit circle and figure out this one:
				if(radius > 60) {
					float offset = std::sqrt(r2-std::abs(pos.X - (x << 4)));
					startY = absoluteRoundUp(pos.Y - offset);
					endY = absoluteRoundUp(pos.Y + offset);
					startZ = absoluteRoundUp(pos.Z - offset);
					endZ = absoluteRoundUp(pos.Z + offset);
				}
				for (s16 y = startY; y < endY;y++) {
					for (s16 z = startZ; z < endZ;z++) {
						SpatialKey key(x,y,z, false);
						if (m_cached.find(key) != m_cached.end()) {
							size_t number_of_entities_in_bucket = m_cached.bucket_size(m_cached.bucket(key));
							if(number_of_entities_in_bucket > 0) {
								m_iterators_stopping_insertion_and_deletion++;
								if(number_of_entities_in_bucket > 3) { // let's search by bucket, might get lucky
									// find the kind of intersection, if any:
									float distance_min = 0;
									float distance_max = 0;
									v3f min_point(x, y, z);
									v3f max_point(x + 16, y + 16, z + 16);
									for(int i = 0; i < 3; i++ ) {
										float a = std::pow(pos[i] - min_point[i],2);
										float b = std::pow(pos[i] - max_point[i],2);
										distance_max += std::max(a, b);
										if( pos[i] < min_point[i] ) distance_min += a; else
										if( pos[i] > max_point[i] ) distance_min += b;
									}
									if (distance_min <= r2) { // at least intersected, if not, ignore all of them, woohoo!
										if (distance_max <= r2) { // everything is inside the radius, woohoo!
											auto range = m_cached.equal_range(key);
											for (auto &it = range.first; it != range.second; ++it) {
												guarunteedCallback(it->second);
											}
										} else { // just an intersection, oh well
											auto range = m_cached.equal_range(key);
											for (auto &it = range.first; it != range.second; ++it) {
												needsCheckedCallback(it->second);
											}
										}
									}
								} else { // not worth checking the bucket first
									auto range = m_cached.equal_range(key);
									for (auto &it = range.first; it != range.second; ++it) {
										needsCheckedCallback(it->second);
									}
								}
								m_iterators_stopping_insertion_and_deletion--;
								handleInsertsAndDeletes();
							}
						}
					}
				}
			}
		} else { // let's just iterate, it'll be faster
			m_iterators_stopping_insertion_and_deletion++;
			for (auto it = m_cached.begin(); it != m_cached.end(); ++it) {
				needsCheckedCallback(it->second);
			}
			m_iterators_stopping_insertion_and_deletion--;
			handleInsertsAndDeletes();
		}
	}
}

void SpatialMap::handleInsertsAndDeletes()
{
	if(!m_iterators_stopping_insertion_and_deletion) {
		if(!m_remove_all) {
			for (auto key : m_pending_deletes) {
				remove(key.padding_or_optional_id, v3f(key.x, key.y, key.z));
			}
			for (auto key : m_pending_inserts) {
				insert(key.padding_or_optional_id, v3f(key.x, key.y, key.z));
			}
		} else {
			m_cached.clear();
			m_remove_all = false;
		}
		m_pending_inserts.clear();
		m_pending_deletes.clear();
	}
}

} // namespace server
