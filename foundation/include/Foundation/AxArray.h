#ifndef AXARRAY_INCLUDE
#define AXARRAY_INCLUDE

#include "Foundation/AxTypes.h"

// TODO(mdeforge): Consider removing SetCapacity and SetSize and renaming things to grow/shrink

/*
    AxArray's are a type-safe dynamic array. They are useful when building a list of things
    with an unknown quantity. When an AxArray resizes, there is a good chance the new array
    will be at a new memory address. AxArray's are based on Sean Barrett's stretchy buffer's,
    [stb_ds.h](https://github.com/nothings/stb/blob/master/stb_ds.h).

    AxArray's are accessed using a regular pointer to the array type.

    int *array = NULL;

    The Size and Capacity of the array is stored in a header before the array itself.

    [Example]
    #include "Foundation/AxArray.h"
    #define AXARRAY_IMPLEMENTATION
    int main(void)
    {
        int *array = NULL;
        ArrayPush(array, 2);
        ArrayPush(array, 3);
        ArrayPush(array, 5);
        for (int i=0; i < ArraySize(array); ++i) {
            printf("%d ", array[i]);
        }
    }
*/

typedef struct AxArrayHeader
{
    size_t Capacity;
    size_t Size;
} AxArrayHeader;

#if defined(AxRealloc) && !defined(AxFree) || !defined(AxRealloc) && defined(AxFree)
#error "You must define both ArrayRealloc and ArrayFree together, or define neither."
#endif
#if !defined(AxRealloc) && !defined(AxFree)
#include <stdlib.h>
#define AxRealloc(c, p, s) realloc(p, s)
#define AxFree(c, p)       free(p)
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void *ArrayReserve(void* A, size_t BlockSize, size_t NewCapacity);

#ifdef __cplusplus
}
#endif

// Returns the header data.
#define ArrayHeader(a)         ((AxArrayHeader *)((uint8_t *)(a) - sizeof(AxArrayHeader)))

// Frees the array
#define ArrayFree(a)           ((void) ((a) ? AxFree(NULL, ArrayHeader(a)) : (void)0), (a) = NULL)

// [Capacity]

// Checks whether the container is empty.
#define ArrayEmpty(a)          (ArraySize(a) > 0 ? false : true)

// Returns the number of elements that can be held in the currently allocated storage.
#define ArrayCapacity(a)       ((a) ? ArrayHeader(a)->Capacity : 0)

// Sets the number of elements that can be held. Reallocates if larger than current capacity.
#define ArraySetCapacity(a, c) (ArrayResize(a, c))

// Returns the number of elements
#define ArraySize(a)           ((a) ? ArrayHeader(a)->Size : 0)

// Sets the number of elements
#define ArraySetSize(a, n)     ((ArrayCapacity(a) < (size_t)(n) ? ArraySetCapacity((a), (size_t)(n)), 0 : 0), (a) ? ArrayHeader(a)->Size = (size_t)(n) : 0)

// Returns the size of the array in bytes (not including the header data).
#define ArraySizeInBytes(a)    (ArraySize(a) * sizeof(*(a)))

#define ArrayNeedsGrowth(a, n) ((n) > ArrayCapacity(a))

// [Element Access]

// Access the first element.
#define ArrayBegin(a)          ((a)[0])

// Returns a pointer one element past the end of the array, or NULL if the array is not allocated.
#define ArrayEnd(a)            ((a) ? (a) + ArraySize(a) : 0)

// Returns a pointer to the last element, or NULL if the array is empty.
#define ArrayBack(a)           ((a) ? ArrayEnd(a) - 1 : 0)

// Pops the last element from the array and returns it
#define ArrayPop(a)            ((a)[--ArrayHeader(a)->Size])

// [Modifiers]

// Adds an element to the end.
#define ArrayPush(a, v)        (ArrayMaybeResize(a, 1), (a)[ArrayHeader(a)->Size++] = (v))

#define ArrayPushArray(a, b) (ArrayMaybeResize(a, ArraySize(a) + ArraySize(b)), memcpy((a) + ArraySize(a), (b), ArraySize(b) * sizeof(*(a))), ArrayHeader(a)->Size += ArraySize(b))

#define ArrayErase(a, i)       (ArrayEraseN(a, i, 1))
#define ArrayEraseN(a, i, n)   (memmove(&(a)[i], &(a)[(i)+(n)], sizeof(*(a)) * (ArrayHeader(a)->Size - (n) - (i))), ArrayHeader(a)->Size -= (n))
#define ArrayEraseSwap(a, i)   ((a)[i] = ArrayBack(a), ArrayHeader(a)->Size -= 1)
#define ArrayMaybeResize(a, n) ((!(a) || ArrayNeedsGrowth(a, n)) ? ArrayResize(a, ArrayCapacity(a) + (n)) : 0)

// Changes the number of elements stored.
#define ArrayResize(a, b)      ((a) = ArrayReserveWrapper((a), sizeof *(a), (b)))

#define ArrayGrowAt(a, n)      (())

// Shrinks the number of elements to the specificed value. Will not reallocate.
#define ArrayShrink(a, n)      ((a) ? ArrayHeader(a)->Size = (n) : 0)

#ifdef __cplusplus
// Reserves storage.
template<class T> static T* ArrayReserveWrapper(T *A, size_t BlockSize, size_t NewCapacity) {
    return (T *)ArrayReserve((void *)A, BlockSize, NewCapacity);
}
#else
// Reserves storage.
#define ArrayReserveWrapper ArrayReserve
#endif

#endif

// [Implementation] ///////////////////////////////////////////

#ifdef AXARRAY_IMPLEMENTATION
// static inline void *ArraySetCapacity(void *A, size_t BlockSize, size_t NewCapacity
// {
//     uint8_t *p = A ? (uint8_t *)ArrayHeader(A) : 0;
//     const size_t BytesBefore = A ? BlockSize * ArrayCapacity(A) + sizeof(AxArrayHeader) : 0;
//     const size_t BytesAfter = NewCapacity ? BlockSize * NewCapacity + sizeof(AxArrayHeader) : 0;

//     // If   
//     if (p && (ArrayCapacity(A) == 0)
//     {
//         // Perform a static allocation

//     }
// }

void *ArrayReserve(void *A, size_t BlockSize, size_t NeededCapacity)
{
    const size_t Capacity = ArrayCapacity(A);
    if (Capacity >= NeededCapacity) {
        return (A);
    }

    const size_t MinNewCapacity = Capacity ? Capacity * 2 : 16;
    const size_t NewCapacity = MinNewCapacity > NeededCapacity ? MinNewCapacity : NeededCapacity;

    void* B = AxRealloc(NULL, (A) ? ArrayHeader(A) : 0, (size_t)NewCapacity * BlockSize + sizeof(AxArrayHeader));
    if (B)
    {
        B = (uint8_t*)B + sizeof(AxArrayHeader);

        if (A == NULL) {
            ArrayHeader(B)->Size = 0;
        }

        ArrayHeader(B)->Capacity = NewCapacity;
    }

    return (B);
}

#endif