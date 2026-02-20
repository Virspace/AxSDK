#pragma once

/**
 * AxCoreSystems.h - Core System Registration for AxScene Plugin
 *
 * Provides functions to create and register all core systems
 * (TransformSystem, RenderSystem, ScriptSystem, and stubs)
 * with the SystemFactory during AxScene plugin load.
 *
 * This is C++ code in the AxScene plugin layer.
 */

/**
 * Register all core system instances with the global SystemFactory.
 * Called during AxScene plugin initialization after standard components
 * have been registered.
 */
void RegisterCoreSystems(void);

/**
 * Unregister and clean up all core system instances.
 * Called during AxScene plugin unload.
 */
void UnregisterCoreSystems(void);
