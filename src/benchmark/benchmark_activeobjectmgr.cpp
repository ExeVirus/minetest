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
	for (size_t i = 0; i < n; i++) {
		auto obj = std::make_unique<TestObject>(randpos());
		bool ok = mgr.registerObject(std::move(obj));
		REQUIRE(ok);
	}
}

}

template <size_t N>
void benchGetObjectsInsideRadius(Catch::Benchmark::Chronometer &meter)
{
	server::ActiveObjectMgr mgr;
	size_t x;
	std::vector<ServerActiveObject*> result;
	mysrand(2010112); // keep the test identical for comparing perf changes

	auto cb = [&x] (ServerActiveObject *obj) -> bool {
		x += obj->m_static_exists ? 0 : 1;
		return false;
	};
	mgr.clear();
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
	mysrand(2010112); // keep the test identical for comparing perf changes

	auto cb = [&x] (ServerActiveObject *obj) -> bool {
		x += obj->m_static_exists ? 0 : 1;
		return false;
	};
	mgr.clear();
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

void benchPseudorandom()
{
	server::ActiveObjectMgr mgr;
	std::vector<ServerActiveObject*> result;
	std::vector<u16> ids;
	result.reserve(200); // don't want mem resizing to be a factor in tests
	mysrand(2010112); // keep the test identical for comparing perf changes

	auto iterationManipulator = [&mgr, &ids] (ServerActiveObject *obj) -> bool {
		//if (obj == nullptr)	return false;
		int val = myrand_range(1, 80); // seldom, just remember this is an expensive callback
		if( val == 1) {
			if(mgr.getActiveObject(obj->getId()-2) != nullptr)
				mgr.removeObject(obj->getId()-2);
		} else if ( val == 2) {
			fill(mgr, 1);
		}
		ids.push_back(obj->getId());
		return false;
	};

	auto inArea = [&ids, &result, &iterationManipulator] (server::ActiveObjectMgr* mgr) {
		ids.clear();
		v3f pos = randpos();
		v3f off(200, 50, 200);
		mgr->getObjectsInArea({pos, pos + off}, result, iterationManipulator);
		return ids.size();
	};

	auto inRadius = [&ids, &result, &iterationManipulator] (server::ActiveObjectMgr* mgr) {
		ids.clear();
		mgr->getObjectsInsideRadius(randpos(), 300.0f, result, iterationManipulator);
		return ids.size();
	};
	
	for (int i = 0; i < 100; i++) {
		mgr.clear();
		fill(mgr, 1000); // start with 1000 to keep things messy

		// Psuedorandom object manipulations
		// Note that the inArea and inRadius checks will manipulate the number of objects
		for (int i = 0; i < 200; i++) {
			switch (myrand_range(0, 2)) {
			case 0:
				for (auto id : ids) {
					auto obj = mgr.getActiveObject(id);
					if(obj != nullptr)
						obj->setBasePosition(randpos());
				}
				break;
			case 1:
				inArea(&mgr);
				break;
			default:
				inRadius(&mgr);
			}
		}
		size_t count = 0;
		mgr.getObjectsInsideRadius(v3f(0, 0, 0), 3000.0, result, [&count](ServerActiveObject* obj) { count++; return false; });
		std::cerr << std::to_string(inArea(&mgr)) + ", " + std::to_string(inRadius(&mgr)) + ", " + std::to_string(count) << std::endl;
	}
	mgr.clear();
}

#define BENCH_INSIDE_RADIUS(_count) \
	BENCHMARK_ADVANCED("inside_radius_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchGetObjectsInsideRadius<_count>(meter); };

#define BENCH_IN_AREA(_count) \
	BENCHMARK_ADVANCED("in_area_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchGetObjectsInArea<_count>(meter); };

#define BENCH_PSEUDORANDOM() \
	BENCHMARK_ADVANCED("psuedorandom")(Catch::Benchmark::Chronometer meter) \
	{ benchPseudorandom(meter); };

TEST_CASE("ActiveObjectMgr") {
	//BENCH_INSIDE_RADIUS(200)
	//BENCH_INSIDE_RADIUS(1450)
	//BENCH_INSIDE_RADIUS(10000)

	//BENCH_IN_AREA(200)
	//BENCH_IN_AREA(1450)
	//BENCH_IN_AREA(10000)

	benchPseudorandom();
}
