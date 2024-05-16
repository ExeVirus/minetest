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

#include <log.h>
#include "mapblock.h"
#include "profiler.h"
#include "activeobjectmgr.h"

namespace server
{

ActiveObjectMgr::~ActiveObjectMgr()
{
	if (!m_active_objects.empty()) {
		warningstream << "server::ActiveObjectMgr::~ActiveObjectMgr(): not cleared."
				<< std::endl;
		clear();
	}
}

void ActiveObjectMgr::clear()
{
	::ActiveObjectMgr<ServerActiveObject>::clear();
	m_spatial_map.removeAll();
}

void ActiveObjectMgr::clearIf(const std::function<bool(ServerActiveObject *, u16)> &cb)
{
	for (auto &it : m_active_objects.iter()) {
		if (!it.second)
			continue;
		if (cb(it.second.get(), it.first)) {
			// Remove reference from m_active_objects
			m_active_objects.remove(it.first);
		}
	}
}

void ActiveObjectMgr::step(
		float dtime, const std::function<void(ServerActiveObject *)> &f)
{
	size_t count = 0;

	for (auto &ao_it : m_active_objects.iter()) {
		if (!ao_it.second)
			continue;
		count++;
		f(ao_it.second.get());
	}

	g_profiler->avg("ActiveObjectMgr: SAO count [#]", count);
}

void ActiveObjectMgr::updateObjectPosition(u16 id, const v3f &last_position, const v3f &new_position)
{
	m_spatial_map.updatePosition(id, last_position, new_position);
}

bool ActiveObjectMgr::registerObject(std::unique_ptr<ServerActiveObject> obj)
{
	assert(obj); // Pre-condition
	if (obj->getId() == 0) {
		u16 new_id = getFreeId();
		if (new_id == 0) {
			errorstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
					<< "no free id available" << std::endl;
			return false;
		}
		obj->setId(new_id);
	} else {
		verbosestream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "supplied with id " << obj->getId() << std::endl;
	}

	if (!isFreeId(obj->getId())) {
		errorstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "id is not free (" << obj->getId() << ")" << std::endl;
		return false;
	}

	if (objectpos_over_limit(obj->getBasePosition())) {
		v3f p = obj->getBasePosition();
		warningstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "object position (" << p.X << "," << p.Y << "," << p.Z
				<< ") outside maximum range" << std::endl;
		return false;
	}

	auto obj_id = obj->getId();
	m_spatial_map.insert(obj_id, obj->getBasePosition());
	m_active_objects.put(obj_id, std::move(obj));

	auto new_size = m_active_objects.size();
	verbosestream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
			<< "Added id=" << obj_id << "; there are now ";
	if (new_size == decltype(m_active_objects)::unknown)
		verbosestream << "???";
	else
		verbosestream << new_size;
	verbosestream << " active objects." << std::endl;
	return true;
}

void ActiveObjectMgr::removeObject(u16 id)
{
	verbosestream << "Server::ActiveObjectMgr::removeObject(): "
			<< "id=" << id << std::endl;

	m_spatial_map.remove(id, m_active_objects.get(id).get()->getBasePosition());

	// this will take the object out of the map and then destruct it
	bool ok = m_active_objects.remove(id);
	if (!ok) {
		infostream << "Server::ActiveObjectMgr::removeObject(): "
				<< "id=" << id << " not found" << std::endl;
	}
}

void ActiveObjectMgr::getObjectsInsideRadius(const v3f &pos, float radius,
		std::vector<ServerActiveObject *> &result,
		std::function<bool(ServerActiveObject *obj)> include_obj_cb)
{
	float r2 = radius * radius;
	aabb3f bounds(pos.X-radius, pos.Y-radius, pos.Z-radius, 
			   pos.X+radius, pos.Y+radius, pos.Z+radius);

	m_spatial_map.getRelevantObjectIds(bounds, [&](u16 id) {
		auto obj = m_active_objects.get(id).get();
		if (!obj) { // should never be hit
			m_spatial_map.remove(id);
			return;
		}
		const v3f &objectpos = obj->getBasePosition();
		if (objectpos.getDistanceFromSQ(pos) > r2)
			return;

		if (!include_obj_cb || include_obj_cb(obj))
			result.push_back(obj);
	});
}

void ActiveObjectMgr::getObjectsInArea(const aabb3f &box,
		std::vector<ServerActiveObject *> &result,
		std::function<bool(ServerActiveObject *obj)> include_obj_cb)
{
	m_spatial_map.getRelevantObjectIds(box,[&](u16 id) {
		auto obj = m_active_objects.get(id).get();
		if (!obj) { // should never be hit
			m_spatial_map.remove(id);
			return;
		}

		const v3f &objectpos = obj->getBasePosition();
		if (!box.isPointInside(objectpos))
			return;

		if (!include_obj_cb || include_obj_cb(obj))
			result.push_back(obj);
	});
}

void ActiveObjectMgr::getAddedActiveObjectsAroundPos(v3f player_pos, f32 radius,
		f32 player_radius, const std::set<u16> &current_objects,
		std::vector<u16> &added_objects)
{
	/*
		Go through the object list,
		- discard removed/deactivated objects,
		- discard objects that are too far away,
		- discard objects that are found in current_objects.
		- add remaining objects to added_objects
	*/
	f32 offset = radius > player_radius ? radius : player_radius;
	aabb3f bounds(player_pos.X-offset, player_pos.Y-offset, player_pos.Z-offset, 
			   player_pos.X+offset, player_pos.Y+offset, player_pos.Z+offset);
	m_spatial_map.getRelevantObjectIds(bounds, [&](u16 id) {
		auto obj = m_active_objects.get(id).get();
		if (!obj) { // should never be hit
			m_spatial_map.remove(id);
			return;
		}
		if (obj->isGone()) {
			return;
		}
		
		f32 distance_f = obj->getBasePosition().getDistanceFrom(player_pos);
		if (obj->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
			// Discard if too far
			if (distance_f > player_radius && player_radius != 0)
				return;
		} else if (distance_f > radius)
			return;

		// Discard if already on current_objects
		auto n = current_objects.find(id);
		if (n != current_objects.end())
			return;

		// Add to added_objects
		added_objects.push_back(id);
	});
}

} // namespace server
