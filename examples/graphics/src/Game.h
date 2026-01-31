#pragma once

#include "AxEngine/AxScript.h"

class Game : public AxScript
{
public:
    void Init() override;
    void Tick(float Alpha, float DeltaT) override;
};

CREATE_SCRIPT(Game, "Game")
