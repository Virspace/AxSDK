#include "AxEngine/AxScripting.h"
#include "AxEngine/AxScript.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxTypes.h"

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

    DLLAPI_ = PlatformAPI_->DLLAPI;

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

    CreateScriptF CreateScriptFunc = reinterpret_cast<CreateScriptF>(DLLAPI_->Symbol(DLL, "CreateScript"));
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

    return (true);
}

bool AxScripting::LoadScripts(const char *ScriptList)
{
    // TODO: Parse script list and load each script
    return (false);
}

bool AxScripting::UnloadScripts()
{
    Term();
    return (true);
}

void AxScripting::Init()
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            info.Script->Init();
        }
    }
}

void AxScripting::Start()
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            info.Script->Start();
        }
    }
}

void AxScripting::FixedTick(const double FixedDeltaT)
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            info.Script->FixedTick(static_cast<float>(FixedDeltaT));
        }
    }
}

void AxScripting::Tick(const double Alpha, const double DeltaT)
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            info.Script->Tick(static_cast<float>(Alpha), static_cast<float>(DeltaT));
        }
    }
}

void AxScripting::LateTick()
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            info.Script->LateTick();
        }
    }
}

void AxScripting::Stop()
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            info.Script->Stop();
        }
    }
}

void AxScripting::Term()
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            info.Script->Term();
            delete info.Script;
            info.Script = nullptr;
        }
        if (DLLAPI_->IsValid(info.DLL)) {
            DLLAPI_->Unload(info.DLL);
        }
    }
    ScriptMap.clear();
}

void AxScripting::SetEngineState(AxCamera* Camera, const AxModelData* Scene)
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            // Friend access allows writing to ReadOnly members
            info.Script->MainCamera.Value_ = Camera;
            info.Script->CurrentScene.Value_ = Scene;
        }
    }
}

void AxScripting::UpdateFrameState(AxVec2 MouseDelta)
{
    for (auto& [name, info] : ScriptMap) {
        if (info.Script) {
            info.Script->MouseDelta = MouseDelta;
        }
    }
}
