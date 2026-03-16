#pragma once

#include "Foundation/AxTypes.h"
#include <unordered_map>
#include <string>

class ScriptBase;

/**
 * ScriptRegistry - Maps script class names to factory functions.
 *
 * Scripts auto-register via AX_IMPLEMENT_SCRIPT's static initializer.
 * In DLL mode, registration happens when the DLL is loaded (static init).
 * In monolithic shipping, registration happens at program startup.
 *
 * The engine queries the registry by name to instantiate scripts.
 *
 * Get() is defined in AxScriptRegistry.cpp (not inline) to ensure a single
 * instance across DLL boundaries. Game.dll resolves it from the host executable.
 */
class ScriptRegistry
{
public:
    using FactoryFn = ScriptBase* (*)();

    static ScriptRegistry& Get();

    void Register(const char* Name, FactoryFn Factory)
    {
        Factories_[Name] = Factory;
    }

    void Unregister(const char* Name)
    {
        Factories_.erase(Name);
    }

    ScriptBase* Create(const char* Name) const
    {
        auto It = Factories_.find(Name);
        if (It != Factories_.end()) {
            return (It->second());
        }
        return (nullptr);
    }

    bool Has(const char* Name) const
    {
        return (Factories_.find(Name) != Factories_.end());
    }

    const std::unordered_map<std::string, FactoryFn>& GetAll() const
    {
        return (Factories_);
    }

    void Clear()
    {
        Factories_.clear();
    }

private:
    ScriptRegistry() = default;
    std::unordered_map<std::string, FactoryFn> Factories_;
};
