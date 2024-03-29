# Axon SDK
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

## Inspirations
* [Handmade Hero](https://handmadehero.org/)
* [Our Machinery](https://ourmachinery.com/)
* [Bitsquid Engine](http://bitsquid.blogspot.com/)
* [Dear ImGui](https://github.com/ocornut/imgui)
* [Mike Acton](https://twitter.com/mike_acton)
* [Niklas Frykholm](https://twitter.com/niklasfrykholm)
* [Sean Barrett](https://twitter.com/nothings)

_"If I have seen further it is by standing on the shoulders of Giants"_ -- Sir Isaac Newton