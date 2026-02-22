#pragma once

#include "AxEngine/AxScriptBase.h"

class Game : public ScriptBase
{
public:
    void OnInit() override;
    void OnUpdate(float DeltaT) override;
};

extern "C" __declspec(dllexport) ScriptBase* CreateNodeScript();
