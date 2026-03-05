# CAR - High Performance CAE PCB Data Relationship System

A high-performance, low-memory CAE software architecture for high-speed PCB design, inspired by KLayout's database architecture.

## Features

- **High Performance Data Management**: ReuseVector memory pool with generation tracking and quarantine semantics
- **Low Memory Footprint**: StringPool for string interning, SmallVector optimization
- **Complete Undo/Redo**: Transaction-based system with add/remove/replace semantics
- **Parametric System**: DBUValue with ExprTk expression library integration
- **Spatial Indexing**: LayerIndex + QuadTree for efficient queries

## Architecture

```
CAR/
├── include/           # Header files
│   ├── car_basic_types.h
│   ├── car_string_pool.h
│   ├── car_param_pool.h
│   ├── car_reuse_vector.h
│   ├── car_shape.h
│   ├── car_pcb_objects.h
│   ├── car_transaction.h
│   ├── car_quadtree.h
│   └── car_database.h
├── src/               # Implementation files
├── test/              # Test programs
└── CMakeLists.txt
```

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## License

MIT License
