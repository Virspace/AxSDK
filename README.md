# Axon SDK
<img style="max-width: 100%; display: inline-block;" alt="gallery" src="https://github.com/user-attachments/assets/69d63972-6a5f-4d2e-87b4-58dd3dede921" />

The Axon SDK (AxSDK) is a set of tools, libraries, documentation, and samples for creating games and simulations of all kinds. My hope is that, when complete, this SDK becomes a fantastic starting point for others to build off of to create new and interesting things with. At least, that's what I plan on using it for!

The Axon SDK is my latest attempt at writing sane, well designed software. It is an opinionated departure from object-oriented programming based on my professional experiences writing fast, maintainable, easy to use code.

Currently, only Windows is supported. However, the APIs are platform agnostic and simply need platform specific implementations.

_Note: This SDK is in active development alongside an editor and a game. The engine core and scene tree are functional, but rendering, physics, and cross-platform support are still in progress._

## Architecture

AxSDK uses a layered language strategy:

* **Foundation (C11)** -- Core data structures, linear math, allocators, platform abstraction, and the API registry. Pure C for ABI stability and easy binding.
* **Plugins (C/C++)** -- Modular shared libraries loaded at runtime. Some are C (AxWindow, AxOpenGL, AxAudio), others are C++ (AxResource, AxLog).
* **Engine (C++)** -- Scene tree, typed node hierarchy, scripting, rendering, and input. Built on top of Foundation and plugins.
* **Game Scripts (C++)** -- ScriptBase subclasses compiled into Game.dll for hot-reload during development, or statically linked into a monolithic executable for shipping.

## Foundation Library

A pure C11 library providing the building blocks everything else is built on:

* **Data Structures** -- AxArray, AxString, AxHashMap, and more
* **Math** -- Vectors, matrices, quaternions, transforms
* **Allocators** -- Unified AxAllocator interface (arena, pool, stack, etc.)
* **Platform** -- File I/O, paths, timing, dynamic library loading
* **API Registry** -- The glue that lets plugins discover and communicate with each other

## Plugins

* **AxWindow** -- Lightweight, render agnostic windowing and input (mouse, keyboard, controller)
* **AxOpenGL** -- Modern OpenGL rendering, shader management, context creation
* **AxResource** -- Handle-based asset management with reference counting and deferred destruction
* **AxLog** -- Structured logging with compile-time level stripping
* **AxAudio** -- Audio playback (early stage, Win32/WASAPI)

## Engine

The engine provides a Godot-style scene tree with typed nodes:

* **SceneTree** -- Hierarchical node management with parent/child/sibling traversal
* **Typed Nodes** -- MeshInstance, CameraNode, LightNode, RigidBody, Collider, AudioSource, and more
* **ScriptBase** -- Per-node behavioral scripting with lifecycle callbacks (OnAttach, OnInit, OnUpdate, OnFixedUpdate, etc.)
* **Renderer** -- OpenGL rendering with GLTF model support
* **Transforms** -- Dirty-list optimization for efficient transform propagation
* **Scene Files** -- Text-based .ats format for git-friendly scene serialization

## Build Modes

* **Debug** -- No optimization, full symbols, shared plugins, all logs, assertions enabled
* **Development** -- Optimized with debug symbols, shared plugins, Game.dll hot-reload enabled
* **Shipping** -- Fully optimized monolithic executable, trace/debug/info logs stripped, assertions disabled

## Features

* Layered C11/C++ architecture with pure C foundation
* Plugin-based architecture for quick and easy extension, modification, or replacement
* Plugins and game scripts can be hot-reloaded at runtime
* Godot-style scene tree with typed node hierarchy
* Per-node scripting with lifecycle management
* Handle-based resource management
* Three-tier build system (Debug, Development, Shipping)
* Text-based scene files (.ats) for version control
* No third-party dependencies in Foundation
* CMake support
* MIT License
* Unit tests

## Design Principles

* Avoid coupling like the plague
* Write data-oriented, cache friendly code
* Extend functionality through plugins, not bloating the foundation
* Keep Foundation pure C11 for ABI stability
* Engine-level concepts (nodes, components, scene tree) live in C++, not Foundation

## Code Organization

* Header files should only contain a types header
* Header files should be written in C for compatibility
* Implementation files can be written in C or C++

## Why C for Foundation?

* C has a stable ABI which allows me to compile DLLs without worrying about compiler versions or flags
* C has a smaller design space than C++ which I find freeing
* C is easy to bind to because you don't have to fight with objects
* I like the challenge, learning new things, and knowing the code I'm using rather than just depending on the STL

## Development Setup

### Prerequisites
- CMake 3.15+
- Clang compiler (C++23 for engine)
- **Windows**: MSYS2 with MinGW-w64
- **Linux**: Standard development tools (planned)
- **macOS**: Xcode command line tools (planned)

### VS Code Setup
This repository includes VS Code configuration for IntelliSense. If you're using a different setup than the defaults, you may need to adjust:

**Windows MSYS2 users**: Set the `MINGW_PREFIX` environment variable:
```bash
export MINGW_PREFIX="/c/msys64/mingw64"  # Adjust path as needed
```

**Other configurations**: Edit `.vscode/c_cpp_properties.json` and update the `compilerPath` for your configuration.

### Build Instructions
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running Tests
```bash
cmake --build build --target FoundationTests
cmake --build build --target AxEngineTests
./build/bin/FoundationTests
./build/bin/AxEngineTests
```

### Assets
You'll need to grab the Sponza Atrium GLTF (.glb) and textures from [CGTrader](https://www.cgtrader.com/free-3d-models/exterior/historic-exterior/sponza-atrium-2022). Put them in _examples/graphics/models_. They should get copied into _build/bin/models_, but if not, put them there until I figure out a better way to manage this demo scene.

## Project Structure

```
AxSDK/
├── Foundation/          # Pure C11 core library
│   ├── include/         # Public headers
│   ├── src/             # Implementation
│   └── tests/           # Foundation tests
├── Plugins/             # Modular plugin system
│   ├── AxWindow/        # Windowing & input (C)
│   ├── AxOpenGL/        # OpenGL rendering (C)
│   ├── AxResource/      # Asset management (C++)
│   ├── AxLog/           # Structured logging (C++)
│   └── AxAudio/         # Audio playback (C, early stage)
├── engine/              # Engine core (C++)
│   ├── include/         # Public engine headers
│   ├── src/             # Engine implementation
│   └── tests/           # Engine tests
└── examples/            # Sample projects
```

## Inspirations
* [Handmade Hero](https://handmadehero.org/)
* [Our Machinery](https://ourmachinery.com/)
* [Bitsquid Engine](http://bitsquid.blogspot.com/)
* [Godot Engine](https://godotengine.org/)
* [Dear ImGui](https://github.com/ocornut/imgui)
* [Mike Acton](https://twitter.com/mike_acton)
* [Niklas Frykholm](https://twitter.com/niklasfrykholm)
* [Sean Barrett](https://twitter.com/nothings)

_"If I have seen further it is by standing on the shoulders of Giants"_ -- Sir Isaac Newton
