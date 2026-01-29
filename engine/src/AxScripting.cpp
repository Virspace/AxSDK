#include "AxEngine/AxScripting.h"
#include "AxEngine/AxScript.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlatform.h"

#include <string>
#include <unordered_map>

typedef AxScript* (*CreateScriptF)();

struct ScriptInfo
{
    std::string Name;
    std::string Path;
    AxDLL DLL;
    AxScript *Script;
};

std::unordered_map<std::string, ScriptInfo> ScriptMap;

bool AxScripting::Create(AxAPIRegistry *APIRegistry)
{
    AXON_ASSERT(APIRegistry);

    APIRegistry_ = APIRegistry;
    PlatformAPI_ = static_cast<AxPlatformAPI *>(APIRegistry_->Get(AXON_PLATFORM_API_NAME));
    if (!PlatformAPI_) {
        return (false);
    }

    DLLAPI_ = PlatformAPI->DLLAPI;

    return (true);
}

void AxScripting::Destroy()
{
    APIRegistry_ = nullptr;
}

bool AxScripting::LoadScript(const char *ScriptPath)
{
    AxDLL DLL = DLLAPI_->Load(ScriptPath);
    if (!DLLAPI_->IsValid(DLL)) {
        return (false);
    }

    CreateScriptF CreateScriptFunc = static_cast<CreateScriptF>(DLLAPI_->Symbol(DLL, "CreateScript"));
    if (!CreateScriptFunc) {
        return (false);
    }

    std::string ScriptName = PlatformAPI_->PathAPI->BaseName(ScriptPath);
    ScriptInfo Info = {
        ScriptName,
        ScriptPath,
        DLL,
        static_cast<AxScript *>(CreateScriptFunc())
    };

    // Share the Registry API with the script
    Info.Script->SetRegistry(APIRegistry_);

    ScriptMap[ScriptName] = Info;
}

bool AxScripting::LoadScripts(const char *ScriptList)
{
    
}

bool AxScripting::UnloadScripts()
{
    
}

void AxScripting::Init()
{
    
}

void AxScripting::Start()
{
    
}

void AxScripting::FixedTick(const double FixedDeltaT)
{
    
}

void AxScripting::Tick(const double Alpha, const double DeltaT)
{
    
}

void AxScripting::LateTick()
{
    
}

void AxScripting::Stop()
{
    
}

void AxScripting::Term()
{
    
}
