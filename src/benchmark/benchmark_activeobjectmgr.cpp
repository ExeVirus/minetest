// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "benchmark_setup.h"
#include "server/activeobjectmgr.h"
#include "util/numeric.h"

namespace {

class TestObject : public ServerActiveObject {
public:
	TestObject(v3f pos) : ServerActiveObject(nullptr, pos)
	{}

	ActiveObjectType getType() const {
		return ACTIVEOBJECT_TYPE_TEST;
	}
	bool getCollisionBox(aabb3f *toset) const {
		return false;
	}
	bool getSelectionBox(aabb3f *toset) const {
		return false;
	}
	bool collideWithObjects() const {
		return true;
	}
};

constexpr float POS_RANGE = 2001;

inline v3f randpos()
{
	return v3f(myrand_range(-POS_RANGE, POS_RANGE),
		myrand_range(-20, 60),
		myrand_range(-POS_RANGE, POS_RANGE));
}

inline void fill(server::ActiveObjectMgr &mgr, size_t n)
{
	mgr.clear();
	for (size_t i = 0; i < n; i++) {
		auto obj = std::make_unique<TestObject>(randpos());
		bool ok = mgr.registerObject(std::move(obj));
		REQUIRE(ok);
	}
}

}

template <size_t N>
void benchInsertObjects(Catch::Benchmark::Chronometer &meter)
{
	server::ActiveObjectMgr mgr;
	
	meter.measure([&] {
		fill(mgr, N);
	});

	mgr.clear(); // implementation expects this
}

template <size_t N>
void benchRemoveObjects(Catch::Benchmark::Chronometer &meter)
{
	server::ActiveObjectMgr mgr;
	
	fill(mgr, N);
	meter.measure([&] {
		mgr.clear();
	});

	mgr.clear();
}

template <size_t N>
void benchUpdateObjectPositions(Catch::Benchmark::Chronometer &meter)
{
	server::ActiveObjectMgr mgr;
	std::vector<v3f> newPositions;
	newPositions.reserve(N);
	
	fill(mgr, N);
	for (size_t i = 0; i < N; i++) {
		newPositions.push_back(randpos());
	}
	meter.measure([&] {
		size_t i = 0;
		mgr.step(0,[&](ServerActiveObject* obj){
			obj->setBasePosition(newPositions.at(i++));
		});
	});
	mgr.clear();
}

template <size_t N>
void benchGetObjectsInsideRadius(Catch::Benchmark::Chronometer &meter)
{
	server::ActiveObjectMgr mgr;
	size_t x;
	std::vector<ServerActiveObject*> result;

	auto cb = [&x] (ServerActiveObject *obj) -> bool {
		x += obj->m_static_exists ? 0 : 1;
		return false;
	};
	fill(mgr, N);
	meter.measure([&] {
		x = 0;
		mgr.getObjectsInsideRadius(randpos(), 30.0f, result, cb);
		return x;
	});
	REQUIRE(result.empty());

	mgr.clear(); // implementation expects this
}

template <size_t N>
void benchGetObjectsInArea(Catch::Benchmark::Chronometer &meter)
{
	server::ActiveObjectMgr mgr;
	size_t x;
	std::vector<ServerActiveObject*> result;

	auto cb = [&x] (ServerActiveObject *obj) -> bool {
		x += obj->m_static_exists ? 0 : 1;
		return false;
	};
	fill(mgr, N);
	meter.measure([&] {
		x = 0;
		v3f pos = randpos();
		v3f off(50, 50, 50);
		off[myrand_range(0, 2)] = 10;
		mgr.getObjectsInArea({pos, pos + off}, result, cb);
		return x;
	});
	REQUIRE(result.empty());

	mgr.clear(); // implementation expects this
}

#define BENCH_INSIDE_RADIUS(_count) \
	BENCHMARK_ADVANCED("inside_radius_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchGetObjectsInsideRadius<_count>(meter); };

#define BENCH_IN_AREA(_count) \
	BENCHMARK_ADVANCED("in_area_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchGetObjectsInArea<_count>(meter); };

#define BENCH_INSERT(_count) \
	BENCHMARK_ADVANCED("insert_objects_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchInsertObjects<_count>(meter); };
#define BENCH_REMOVE(_count) \
	BENCHMARK_ADVANCED("remove_objects_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchRemoveObjects<_count>(meter); };
#define BENCH_UPDATE(_count) \
	BENCHMARK_ADVANCED("update_objects_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchUpdateObjectPositions<_count>(meter); };

TEST_CASE("ActiveObjectMgr") {
	BENCH_INSERT(10000)

	BENCH_REMOVE(10000)
	
	BENCH_UPDATE(10000)
	
	BENCH_INSIDE_RADIUS(200)
	BENCH_INSIDE_RADIUS(1450)
	BENCH_INSIDE_RADIUS(10000)

	BENCH_IN_AREA(200)
	BENCH_IN_AREA(1450)
	BENCH_IN_AREA(10000)
}
