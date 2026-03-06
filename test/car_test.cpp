#include <cassert>
#include <iostream>
#include <vector>

#include "car_database.h"
#include "car_transaction.h"

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
    db.insert(l, "insert_layer");

    Trace t{};
    t.name_id = db.strings.intern("CLK_MAIN");
    t.layer_id = 1;
    t.segments.push_back(LineSegment(Point(0, 0), Point(100, 0), 10));

    ObjectId id = db.insert(t, "insert_trace");
    assert(db.traces.get(id) != nullptr);

    auto matches = db.find_by_name_prefix("CLK");
    assert(!matches.empty());

    Trace t2 = t;
    t2.name_id = db.strings.intern("CLK_AUX");
    assert(db.replace(id, t2, "replace_trace"));
    assert(db.can_undo());

    assert(db.undo());
    assert(db.traces.get(id) != nullptr);
    assert(db.redo());

    assert(db.erase(id, PCBDatabase::EntityKind::TRACE, "erase_trace"));
    assert(db.traces.get(id) == nullptr);
    assert(db.undo());
    assert(db.traces.get(id) != nullptr);
}


void test_database_via_generic_ops() {
    PCBDatabase& db = PCBDatabase::get_instance();
    db.clear();

    Via v{};
    v.name_id = db.strings.intern("VIA_CLK");
    v.start_layer = 1;
    v.end_layer = 8;

    ObjectId via_id = db.insert(v, "insert_via");
    assert(db.vias.get(via_id) != nullptr);

    Via v2 = v;
    v2.name_id = db.strings.intern("VIA_CLK_NEW");
    v2.start_layer = 2;
    assert(db.replace(via_id, v2, "replace_via"));

    auto matches = db.find_by_name_prefix("VIA_");
    assert(!matches.empty());

    assert(db.erase(via_id, PCBDatabase::EntityKind::VIA, "erase_via"));
    assert(db.vias.get(via_id) == nullptr);
    assert(db.undo());
    assert(db.vias.get(via_id) != nullptr);
}


void test_database_batch_transaction() {
    PCBDatabase& db = PCBDatabase::get_instance();
    db.clear();

    Trace t{};
    t.name_id = db.strings.intern("BATCH_T");
    t.layer_id = 1;

    Via v{};
    v.name_id = db.strings.intern("BATCH_V");
    v.start_layer = 1;
    v.end_layer = 2;

    db.begin("batch_insert");
    ObjectId tid = db.insert(t);
    ObjectId vid = db.insert(v);
    db.commit();

    assert(db.traces.get(tid) != nullptr);
    assert(db.vias.get(vid) != nullptr);

    assert(db.undo());
    assert(db.traces.get(tid) == nullptr);
    assert(db.vias.get(vid) == nullptr);
    assert(db.redo());
    assert(db.traces.get(tid) != nullptr);
    assert(db.vias.get(vid) != nullptr);
}


void test_transaction_manager_log_only() {
    TransactionManager tm;
    tm.begin("tx1");
    tm.record(TxOp{TxOpType::INSERT, 1, 100, 0xFFFFFFFFu, 0});
    tm.record(TxOp{TxOpType::ERASE, 2, 200, 1, 0xFFFFFFFFu});
    tm.commit();

    assert(tm.can_undo());
    TxRecord tx;
    assert(tm.pop_undo(tx));
    assert(tx.ops.size() == 2);
    tm.push_redo(std::move(tx));
    assert(tm.can_redo());
}

int main() {
    cout << "Running CAR tests..." << endl;
    test_string_pool();
    test_reuse_vector();
    test_shape_manager();
    test_quadtree();
    test_database_transaction_and_search();
    test_database_via_generic_ops();
    test_database_batch_transaction();
    test_transaction_manager_log_only();
    cout << "All tests passed." << endl;
    return 0;
}
