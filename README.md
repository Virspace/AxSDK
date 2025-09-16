# Axon SDK
<img style="max-width: 100%; display: inline-block;" alt="gallery" src="https://github.com/user-attachments/assets/69d63972-6a5f-4d2e-87b4-58dd3dede921" />

The Axon SDK (AxSDK) is a set of tools, libraries, documentation, and samples for creating games and simulations of all kinds. My hope is that, when complete, this SDK becomes a fantastic starting point for others to build off of to create new and interesting things with. At least, that's what I plan on using it for!

The Axon SDK is my latest attempt at writing sane, well designed software. It is an opinionated departure from object-oriented programming based on my professional experiences writing fast, maintainable, easy to use code.

Currently, only Windows 8.1+ is supported. However, the API's are platform agnostic and simply need platform specific implementations.

_Note: This SDK is in a very early state and is being developed alongside an editor and a game! AxWindow is probably the most stable plugin to date but might be missing some features. There is much to do in AxOpenGL, and even the Foundation library will likely see some big changes._

Libraries
* Foundation - A collection of data structures and algorithms, linear math, and the all important API registry
* Drawable - A set of data structures and functions that help assemble a low-level list of polygons into draw commands

Plugins
* AxWindow - A lightweight, render agnostic, multi-platform (planned), API for creating windows and managing input (mouse, keyboard, controller)
* AxOpenGL - A simple, multi-platform (planned), OpenGL wrapper

Features
* Written in C11
* Plugin-based architecture for quick and easy extension, modification, or replacement
* Plugins can be loaded and hot-reloaded at runtime
* No third-party dependencies
* CMake Support
* MIT License
* Unit Tests (Limited coverage, for now)

Design Principles
* Avoid coupling like the plague
* Write data-oriented, cache friendly code
* Extend functionality through plugins, not bloating the foundation

Code Organization
* Header files should only contain a types header
* Header files should be written in C for compatibility
* Implementation files can be written in C or C++

[Axon SDK Roadmap](https://trello.com/b/a9z3gJWq)

## Why C?
* C has a stable ABI which allows me to compile DLLs without worrying about compiler versions or flags
* C has a smaller design space than C++ which I find freeing
* C is easy to bind to because you don't have to fight with objects
* I like the challenge, learning new things, and knowing the code I'm using rather than just depending on the STL

## Development Setup

### Prerequisites
- CMake 3.15+
- Clang compiler
- **Windows**: MSYS2 with MinGW-w64
- **Linux**: Standard development tools
- **macOS**: Xcode command line tools

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

### Assets
You'll need to grab the Sponza Atrium GLTF (.glb) and textures from [CGTrader]([url](https://www.cgtrader.com/free-3d-models/exterior/historic-exterior/sponza-atrium-2022)). Put them in _examples\graphics\models_. They should get copied into the _build\bin\models_, but if not, put them there until I figure out a better way to manage this demo scene.

## Inspirations
* [Handmade Hero](https://handmadehero.org/)
* [Our Machinery](https://ourmachinery.com/)
* [Bitsquid Engine](http://bitsquid.blogspot.com/)
* [Dear ImGui](https://github.com/ocornut/imgui)
* [Mike Acton](https://twitter.com/mike_acton)
* [Niklas Frykholm](https://twitter.com/niklasfrykholm)
* [Sean Barrett](https://twitter.com/nothings)

_"If I have seen further it is by standing on the shoulders of Giants"_ -- Sir Isaac Newton
