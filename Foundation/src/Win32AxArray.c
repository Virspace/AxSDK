#include "AxArray.h"

int32_t _GrowCapacity(AxArray *Array, int32_t Size)
{
    Assert(Array && "_GrowCapacity passed an empty AxArray!");

    int32_t NewCapacity = Array->Capacity ? (Array->Capacity + Array->Capacity / 2) : 8; return ((NewCapacity > Size) ? NewCapacity : Size);
}

void AxArrayCreate(AxArray *Array)
{
    Assert(Array && "AxArrayCreate passed an empty AxArray!");

    Array->Size = 0;
    Array->Capacity =0 ;
    Array->Data = NULL;
}

void AxArrayDestroy(AxArray *Array)
{
    Assert(Array && "AxArrayDestroy passed an empty AxArray!");

    if (Array->Data) {
        SAFE_DELETE(Array->Data);
    }
}

void AxArrayClear(AxArray *Array)
{
    Assert(Array && "AxArrayClear passed an empty AxArray!");

    if (Array->Data)
    {
        Array->Size = 0;
        Array->Capacity = 0;
        SAFE_DELETE(Array->Data);
    }
}

bool AxArrayIsEmpty(const AxArray *Array)
{
    Assert(Array && "AxArrayIsEmpty passed an empty AxArray!");

    return (Array->Size == 0);
}

int32_t AxArraySize(const AxArray *Array)
{
    Assert(Array && "AxArraySize passed an empty AxArray!");

    return (Array->Size);
}

int32_t AxArraySizeInBytes(const AxArray *Array)
{
    Assert(Array && "AxArraySizeInBytes passed an empty AxArray!");

    return (Array->Size * Array->_BlockSize);
}

int32_t AxArrayMaxSize(const AxArray *Array)
{
    Assert(Array && "AxArrayMaxSize passed an empty AxArray!");

    return (0x7FFFFFFF / Array->_BlockSize);
}

int32_t AxArrayCapacity(const AxArray *Array)
{
    Assert(Array && "AxArrayCapacity passed an empty AxArray!");

    return (Array->Capacity);
}

void AxArrayResize(AxArray *Array, int32_t NewSize)
{
    Assert(Array && "AxArrayResize passed an empty AxArray!");

    if (NewSize > Array->Capacity)
    {
        AxArrayReserve(Array, _GrowCapacity(Array, NewSize));
        Array->Size = NewSize;
    }
}

void AxArrayReserve(AxArray *Array, int32_t NewCapacity)
{
    Assert(Array && "AxArrayReserve passed an empty AxArray!");

    if (NewCapacity <= Array->Capacity) {
        return;
    }

    void *NewData = malloc((size_t)NewCapacity * Array->_BlockSize);
    if (Array->Data)
    {
        memcpy(NewData, Array->Data, (size_t)Array->Size * sizeof(Array->_BlockSize));
        SAFE_DELETE(Array->Data);
        Array->Data = NewData;
        Array->Capacity = NewCapacity;
    }
}

void *AxArrayAt(const AxArray *Array, int32_t Index)
{
    Assert(Array && "AxArrayAt passed an empty AxArray!");
    Assert(Index >= 0 && Index < Array->Size);

    return ((uint8_t)Array->Data + Index);
}

void AxArrayPushFront(AxArray *Array, void *Value)
{
    Assert(Array && "AxArrayPushFront passed an empty AxArray!");

    if (Array->Size == 0) {
        AxArrayPushBack(Array, Value);
    } else {
        AxArrayInsert(Array, Array->Data, Value);
    }
}

void AxArrayPushBack(AxArray *Array, void *Value)
{
    Assert(Array && "AxArrayPushBack passed an empty AxArray!");

    if (Array->Size == Array->Capacity) {
        AxArrayReserve(Array, _GrowCapacity(Array, Array->Size + 1));
    }

    memcpy((uint8_t)Array->Data + Array->Size, &Value, Array->_BlockSize);
    Array->Size++;
}

void *AxArrayInsert(AxArray *Array, const void *It, const void *Value)
{
    Assert(Array && "AxArrayInsert passed an empty AxArray!");
    Assert(It >= Array->Data && It <= (uint8_t)Array->Data + Array->Size);

    const ptrdiff_t Offset = (uint8_t)It - (uint8_t)Array->Data;
    if (Array->Size == Array->Capacity) {
        AxArrayReserve(Array, _GrowCapacity(Array, Array->Size + 1));
    }

    if (Offset < Array->Size) {
        memmove(
            (uint8_t)Array->Data + Offset + 1,
            (uint8_t)Array->Data + Offset,
            ((size_t)Array->Size - (size_t)Offset) * Array->_BlockSize
        );
    }

    memcpy(&Array->Data + Offset, &Value, Array->_BlockSize);
    Array->Size++;

    return ((uint8_t)Array->Data + Offset);
}
