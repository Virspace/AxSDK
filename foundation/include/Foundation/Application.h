#pragma once

#include "Foundation/Types.h"

#define AXON_APPLICATION_API_NAME "AxonApplicationAPI"

typedef struct AxApplication AxApplication;

struct AxApplicationAPI
{
    AxApplication *(*Create)(int argc, char **argv);
    bool (*Tick)(AxApplication *App);
    void (*Destroy)(AxApplication *App);
};