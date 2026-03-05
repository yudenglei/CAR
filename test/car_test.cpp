#include <cassert>
#include <iostream>
#include <vector>

#include "car_database.h"

using namespace std;

void test_string_pool() {
    StringPool& pool = StringPool::get_instance();
    pool.clear();
    StringId id1 = pool.intern("test_string");
    StringId id2 = pool.intern("test_string");
    assert(id1 == id2);
    assert(pool.get(id1) == "test_string");
}

void test_reuse_vector() {
    ReuseVector<int> vec;
    ObjectId h1 = vec.add(10);
    ObjectId h2 = vec.add(20);
    assert(*vec.get(h1) == 10);
    vec.remove(h2);
    ObjectId h3 = vec.add(30);
    assert(vec.size() == 2);
    assert(vec.get(h2) == nullptr);
    assert(*vec.get(h3) == 30);
}

void test_shape_manager() {
    ShapeManager& sm = ShapeManager::get_instance();
    sm.clear();

    ShapeId bid = sm.add(Box(0, 0, 100, 100));
    ShapeId cid = sm.add(Circle(10, 10, 5));
    assert(sm.valid(bid));
    assert(sm.valid(cid));
    sm.remove(bid);
    assert(!sm.valid(bid));
}

void test_quadtree() {
    QuadTree qt(BBox(0, 0, 1000, 1000), 0);
    qt.insert({1, BBox(10, 10, 20, 20)});
    qt.insert({2, BBox(200, 200, 300, 300)});

    vector<ObjectId> out;
    qt.query(BBox(0, 0, 25, 25), out);
    assert(out.size() == 1 && out[0] == 1);
}

void test_database_transaction_and_search() {
    PCBDatabase& db = PCBDatabase::get_instance();
    db.clear();

    Layer l{};
    l.name_id = db.strings.intern("Top");
    db.add_layer(l);

    Trace t{};
    t.name_id = db.strings.intern("CLK_MAIN");
    t.layer_id = 1;
    t.segments.push_back(LineSegment(Point(0, 0), Point(100, 0), 10));

    ObjectId id = db.add_trace(1, t);
    assert(db.traces.get(id) != nullptr);

    auto matches = db.find_by_name_prefix("CLK");
    assert(!matches.empty());

    Trace t2 = t;
    t2.name_id = db.strings.intern("CLK_AUX");
    assert(db.replace_trace(id, t2));
    assert(db.can_undo());

    assert(db.undo());
    assert(db.traces.get(id) != nullptr);
    assert(db.redo());

    assert(db.remove_trace(id));
    assert(db.traces.get(id) == nullptr);
    assert(db.undo());
    assert(db.traces.get(id) != nullptr);
}

int main() {
    cout << "Running CAR tests..." << endl;
    test_string_pool();
    test_reuse_vector();
    test_shape_manager();
    test_quadtree();
    test_database_transaction_and_search();
    cout << "All tests passed." << endl;
    return 0;
}
