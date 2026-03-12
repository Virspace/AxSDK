#include "AxEngine/AxScriptRegistry.h"

ScriptRegistry& ScriptRegistry::Get()
{
    static ScriptRegistry s_Instance;
    return (s_Instance);
}
