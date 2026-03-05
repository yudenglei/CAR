/**
 * @file car_test.cpp
 * @brief Test program for CAR core library
 */

#include <iostream>
#include <cassert>
#include <cstdint>
#include "car_basic_types.h"
#include "car_string_pool.h"
#include "car_reuse_vector.h"
#include "car_shape.h"
#include "car_pcb_objects.h"
#include "car_database.h"
#include "car_quadtree.h"

using namespace std;

void test_basic_types() {
    cout << "Testing basic types..." << endl;
    
    // Test DBUValue
    DBUValue v1(100.5);
    assert(!v1.is_parametric());
    assert(v1.value == 100.5);
    
    DBUValue v2(ExprId(1000));
    assert(v2.is_parametric());
    assert(v2.expr_id == 1000);
    
    // Test Point
    Point p1(10, 20);
    assert(p1.x.value == 10);
    assert(p1.y.value == 20);
    
    // Test Box
    Box b(0, 0, 100, 100);
    assert(b.x1.value == 0);
    assert(b.y1.value == 0);
    
    cout << "Basic types: PASSED" << endl;
}

void test_string_pool() {
    cout << "Testing string pool..." << endl;
    
    StringPool& pool = StringPool::get_instance();
    pool.clear();
    
    StringId id1 = pool.intern("test_string");
    assert(id1 != 0);
    
    StringId id2 = pool.intern("test_string");
    assert(id1 == id2);  // Same string should return same ID
    
    StringId id3 = pool.intern("another_string");
    assert(id3 != id1);
    
    assert(pool.get(id1) == "test_string");
    assert(pool.get(id3) == "another_string");
    
    cout << "String pool: PASSED" << endl;
}

void test_reuse_vector() {
    cout << "Testing reuse vector..." << endl;
    
    ReuseVector<int> vec;
    
    // Add items
    ObjectId h1 = vec.add(10);
    ObjectId h2 = vec.add(20);
    ObjectId h3 = vec.add(30);
    
    assert(vec.size() == 3);
    assert(*vec.get(h1) == 10);
    assert(*vec.get(h2) == 20);
    assert(*vec.get(h3) == 30);
    
    // Remove and reuse
    vec.remove(h2);
    assert(vec.size() == 2);
    assert(vec.get(h2) == nullptr);
    
    // Add after remove (should reuse slot)
    ObjectId h4 = vec.add(40);
    assert(vec.size() == 3);
    
    cout << "Reuse vector: PASSED" << endl;
}

void test_shape_manager() {
    cout << "Testing shape manager..." << endl;
    
    ShapeManager& sm = ShapeManager::get_instance();
    
    // Add shapes
    Box b(0, 0, 100, 100);
    ShapeId bid = sm.add_box(b);
    assert(bid != 0);
    
    Circle c(50, 50, 25);
    ShapeId cid = sm.add_circle(c);
    assert(cid != 0);
    
    // Get shapes
    Box* b2 = sm.get_box(bid);
    assert(b2 != nullptr);
    
    Circle* c2 = sm.get_circle(cid);
    assert(c2 != nullptr);
    
    // Get type
    assert(sm.get_type(bid) == ShapeType::BOX);
    assert(sm.get_type(cid) == ShapeType::CIRCLE);
    
    cout << "Shape manager: PASSED" << endl;
}

void test_pcb_objects() {
    cout << "Testing PCB objects..." << endl;
    
    // Test Trace
    Trace trace;
    trace.name_id = StringPool::get_instance().intern("GND");
    trace.layer_id = 1;
    trace.start_cap = CapType::ROUND;
    trace.end_cap = CapType::ROUND;
    trace.join = JoinType::MITER;
    
    LineSegment seg1(Point(0, 0), Point(100, 0), DBUValue(50));
    trace.segments.push_back(seg1);
    
    assert(trace.segments.size() == 1);
    assert(!trace.has_arc());
    
    // Test Via
    Via via;
    via.name_id = StringPool::get_instance().intern("VIA1");
    via.position = Point(50, 50);
    via.start_layer = 1;
    via.end_layer = 4;
    
    // Test Component
    Component comp;
    comp.refdes = StringPool::get_instance().intern("R1");
    comp.desc_id = StringPool::get_instance().intern("Resistor");
    comp.position = Point(1000, 2000);
    comp.rotation = 90.0;
    
    cout << "PCB objects: PASSED" << endl;
}

void test_quadtree() {
    cout << "Testing quadtree..." << endl;
    
    BBox bounds(0, 0, 10000, 10000);
    QuadTree qt(bounds, 0);
    
    // Insert objects
    QuadObject obj1{1, BBox(100, 100, 200, 200)};
    QuadObject obj2{2, BBox(500, 500, 600, 600)};
    QuadObject obj3{3, BBox(150, 150, 250, 250)};
    
    qt.insert(obj1);
    qt.insert(obj2);
    qt.insert(obj3);
    
    // Query
    vector<ObjectId> results;
    BBox query_area(0, 0, 300, 300);
    qt.query(query_area, results);
    
    assert(results.size() == 2);  // obj1 and obj3
    
    // Point query
    results.clear();
    qt.query_point(Point(550, 550), results);
    assert(results.size() == 1);
    assert(results[0] == 2);
    
    cout << "QuadTree: PASSED" << endl;
}

void test_database() {
    cout << "Testing database..." << endl;
    
    PCBDatabase& db = PCBDatabase::get_instance();
    db.clear();
    
    // Add a layer
    Layer layer;
    layer.name_id = db.strings.intern("Top Copper");
    layer.number = 1;
    layer.type = Layer::Type::SIGNAL;
    ObjectId layer_id = db.add_layer(layer);
    
    // Add a trace
    Trace trace;
    trace.name_id = db.strings.intern("Trace1");
    trace.layer_id = 1;
    LineSegment seg(Point(0, 0), Point(1000, 0), DBUValue(100));
    trace.segments.push_back(seg);
    
    ObjectId trace_id = db.add_trace(1, trace);
    assert(trace_id != 0);
    
    // Test undo
    assert(db.can_undo() == true);
    db.undo();
    assert(db.can_undo() == false);
    
    // Test redo
    assert(db.can_redo() == true);
    db.redo();
    assert(db.can_undo() == true);
    
    cout << "Database: PASSED" << endl;
}

int main() {
    cout << "=== CAR Core Library Tests ===" << endl << endl;
    
    test_basic_types();
    test_string_pool();
    test_reuse_vector();
    test_shape_manager();
    test_pcb_objects();
    test_quadtree();
    test_database();
    
    cout << endl << "=== All Tests PASSED ===" << endl;
    return 0;
}
