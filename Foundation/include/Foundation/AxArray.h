#pragma once

#include "Foundation/Types.h"

typedef struct AxArray AxArray;

#define AX_ARRAY(a) AxArray *a = NULL; AxArrayCreate(a);

void AxArrayCreate(AxArray *Array);
void AxArrayDestroy(AxArray *Array);
void AxArrayClear(AxArray *Array);
bool AxArrayIsEmpty(const AxArray *Array);
int32_t AxArraySize(const AxArray *Array);
int32_t AxArraySizeInBytes(const AxArray *Array);
int32_t AxArrayMaxSize(const AxArray *Array);
int32_t AxArrayCapacity(const AxArray *Array);

void AxArrayResize(AxArray *Array, int32_t NewSize);
void AxArrayReserve(AxArray *Array, int32_t NewCapacity);

void *AxArrayAt(const AxArray *Array, int32_t Index);
void AxArrayPushFront(AxArray *Array, const void *Value);
void AxArrayPushBack(AxArray *Array, const void *Value);
void *AxArrayInsert(AxArray *Array, const void *It, const void *Value);