// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "benchmark_setup.h"
#include "server/activeobjectmgr.h"
#include "util/numeric.h"
#include <array>
#include <chrono>

namespace {

	class TestObject : public ServerActiveObject {
	public:
		TestObject(v3f pos) : ServerActiveObject(nullptr, pos)
		{}

		ActiveObjectType getType() const {
			return ACTIVEOBJECT_TYPE_TEST;
		}
		bool getCollisionBox(aabb3f* toset) const {
			return false;
		}
		bool getSelectionBox(aabb3f* toset) const {
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

	inline void fill(server::ActiveObjectMgr& mgr, size_t n)
	{
		for (size_t i = 0; i < n; i++) {
			auto obj = std::make_unique<TestObject>(randpos());
			bool ok = mgr.registerObject(std::move(obj));
			REQUIRE(ok);
		}
	}

}

template <size_t N>
void benchGetObjectsInsideRadius(Catch::Benchmark::Chronometer& meter)
{
	server::ActiveObjectMgr mgr;
	size_t x;
	std::vector<ServerActiveObject*> result;
	mysrand(2010112); // keep the test identical for comparing perf changes

	auto cb = [&x](ServerActiveObject* obj) -> bool {
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
void benchGetObjectsInArea(Catch::Benchmark::Chronometer& meter)
{
	server::ActiveObjectMgr mgr;
	size_t x;
	std::vector<ServerActiveObject*> result;
	mysrand(2010112); // keep the test identical for comparing perf changes

	auto cb = [&x](ServerActiveObject* obj) -> bool {
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
		mgr.getObjectsInArea({ pos, pos + off }, result, cb);
		return x;
		});
	REQUIRE(result.empty());

	mgr.clear(); // implementation expects this
}

void benchPseudorandom()
{
	server::ActiveObjectMgr mgr;
	std::vector<ServerActiveObject*> result;
	int numToAdd = 0;
	std::vector<u16> ids;
	bool debug = false;
	int deleteEm = 0;
	result.reserve(200); // don't want mem resizing to be a factor in tests
	mysrand(2010112); // keep the test identical for comparing perf changes

	auto iterationManipulator = [&mgr, &ids, &deleteEm, &numToAdd](ServerActiveObject* obj) -> bool {
		//if (obj == nullptr)	return false;
		//int val = myrand_range(1, 80); // seldom, just remember this is an expensive callback
		if (deleteEm == 10) {
			if (obj != nullptr) {
				mgr.removeObject(obj->getId());
			}
			return false;
		}
		else if (deleteEm == 20) {
			numToAdd += 1;
			//std::cout << "add()" << std::endl;
		}
		ids.push_back(obj->getId());
		return false;
		};

	auto inArea = [&ids, &result, &iterationManipulator, &deleteEm, &debug](server::ActiveObjectMgr* mgr) {
		ids.clear();
		v3f pos = randpos();
		v3f off(200, 50, 200);
		deleteEm = myrand_range(1, 80);
		mgr->getObjectsInArea({ pos, pos + off }, result, iterationManipulator);
		//if (debug) {
		//	std::cout << "::area::" << std::endl;
		//	for (auto id : ids) {
		//		auto obj = mgr->getActiveObject(id);
		//		if (obj != nullptr) {
		//			v3f& pos = obj->getBasePosition();
		//			std::cout << obj->getId() << " : " << pos.X << "," << pos.Y << "," << pos.Z << std::endl;
		//		}
		//	}
		//}
		return ids.size();
		};
	auto inRadius = [&ids, &result, &iterationManipulator, &deleteEm, &debug](server::ActiveObjectMgr* mgr) {
		ids.clear();
		v3f pos = randpos();
		deleteEm = myrand_range(1, 80);
		//std::cout << "pos(" << pos.X << "," << pos.Y << "," << pos.Z << ")" << std::endl;
		mgr->getObjectsInsideRadius(pos, 300.0f, result, iterationManipulator);
		//if (debug) {
		//	std::cout << "::radius::" << std::endl;
		//	for (auto id : ids) {
		//		auto obj = mgr->getActiveObject(id);
		//		if (obj != nullptr) {
		//			v3f& pos = obj->getBasePosition();
		//			std::cout << obj->getId() << " : " << pos.X << "," << pos.Y << "," << pos.Z << std::endl;
		//		}
		//	}
		//}
		return ids.size();
		};

	for (int i = 0; i < 2; i++) {
		mgr.clear();
		fill(mgr, 15000); // start with 1000 to keep things messy
		//mgr.getObjectsInsideRadius(v3f(0, 0, 0), 3000.0, result, [](ServerActiveObject* obj) { std::cout << obj->getId() << std::endl; return false; });

		// Psuedorandom object manipulations
		// Note that the inArea and inRadius checks will manipulate the number of objects
		auto start_time = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < 20000; i++) {
			size_t var = 0;
			switch (myrand_range(0, 2)) {
			case 0:
				std::cout << "for(" << ids.size() << ")" << std::endl;
				for (auto id : ids) {
					auto obj = mgr.getActiveObject(id);
					if (obj != nullptr) {
						v3f newPos(obj->getBasePosition());
						std::swap(newPos.X, newPos.Y);
						std::swap(newPos.Y, newPos.Z);
						obj->setBasePosition(newPos);
					}
				}
				break;
			case 1:
				var = inArea(&mgr);
				std::cout << "area(" << var << ")" << std::endl;
				break;
			default:
				var = inRadius(&mgr);
			}
			if (numToAdd > 0) {
				fill(mgr, numToAdd);
				numToAdd = 0;
			}
		}
		size_t count = 0;
		mgr.getObjectsInsideRadius(v3f(0, 0, 0), 3000.0, result, [&count, &ids](ServerActiveObject* obj) { count++; return false; });
		auto end_time = std::chrono::high_resolution_clock::now();
		auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
		double elapsed_seconds = static_cast<double>(elapsed_ns.count()) / 1e9;
		double elapsed_milliseconds = static_cast<double>(elapsed_ns.count()) / 1e6;
		std::cout << "Elapsed time: " << elapsed_milliseconds << " milliseconds\n";
		debug = true;
		std::cout << std::to_string(inArea(&mgr)) + ", " + std::to_string(inRadius(&mgr)) + ", " + std::to_string(count) << std::endl;
		debug = false;
	}
	mgr.clear();
}

void radiusTest(v3f origin, float distance) {
	server::ActiveObjectMgr mgr;
	std::vector<ServerActiveObject*> _;
	std::array<v3f, 9> positions = {
		v3f(0.0,0.0f,0.0f),
		v3f(0.0,0.0f,0.0f),
		v3f(0.0,0.0f,0.0f),
		v3f(0.0,0.0f,0.0f),
		v3f(0.0,0.0f,0.0f),
		v3f(0.0,0.0f,0.0f),
		v3f(0.0,0.0f,0.0f),
		v3f(0.0,0.0f,0.0f),
		v3f(0.0,0.0f,0.0f),
	};

	for (auto pos : positions)
		mgr.registerObject(std::move(std::make_unique<TestObject>(pos)));
	size_t count = 0;
	mgr.getObjectsInsideRadius(origin, distance, _, [&count](ServerActiveObject* obj) { count++; return false; });

	std::cout << "Radius test: " << origin.X << "," << origin.Y << "," << origin.Z << " - " << distance << " returned: " << count << " entities." << std::endl;
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
	//radiusTest(v3f(0, 0, 0), 1);
	//radiusTest(v3f(16, 0, 0), 16);
	//radiusTest(v3f(0, 16, 0), 16);
	//radiusTest(v3f(0, 0, 16), 16);
	//radiusTest(v3f(-16, 0, 0), 16);
	//radiusTest(v3f(0, -16, 0), 16);
	//radiusTest(v3f(0, 0, -16), 16);
}
