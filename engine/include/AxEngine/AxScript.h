#pragma once

#include "Foundation/AxTypes.h"

struct AxAPIRegistry;

/**
* Interface class that all AxScripts should derive from.
*/
class AxScript
{
public:
	virtual ~AxScript() {}
    virtual void Startup() = 0;
    virtual void Shutdown() = 0;

	inline void SetRegistry(const AxAPIRegistry *APIRegistry) { APIRegistry_ = APIRegistry; }

private:
	const AxAPIRegistry *APIRegistry_{nullptr};
};

#define CREATE_SCRIPT( ScriptClass, ScriptName ) \
		extern "C" __declspec(dllexport) AxScript* CreateScript() \
		{ \
			return new ScriptClass(); \
		} \
