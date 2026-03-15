#pragma once

/**
 * AxSignal.h - Node-scoped signal system types
 *
 * Provides SignalArg/SignalArgs for typed argument passing, and
 * SignalConnection/SignalSlot for per-node signal storage.
 * Signals are implicit -- emitting a signal name creates the slot.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

//=============================================================================
// Forward Declarations
//=============================================================================

class Node;
struct SignalArgs;

//=============================================================================
// SignalArg — Discriminated union for a single signal argument
//=============================================================================

#define AX_SIGNAL_MAX_ARGS 4

struct SignalArg
{
  enum class Type : uint8_t { None, Float, Int, Bool, String, Pointer };

  Type ArgType{Type::None};
  union {
    float AsFloat;
    int32_t AsInt;
    bool AsBool;
    const char* AsString;  // non-owning, must outlive signal dispatch
    void* AsPointer;
  };
};

//=============================================================================
// SignalArgs — Fixed-capacity container of signal arguments
//=============================================================================

struct SignalArgs
{
  SignalArg Args[AX_SIGNAL_MAX_ARGS];
  uint8_t Count{0};

  /** Type-safe argument access. Returns default value if index out of bounds. */
  template<typename T> T Get(uint8_t Index) const;
};

// Specializations
template<> inline float SignalArgs::Get<float>(uint8_t Index) const
{
  if (Index >= Count || Args[Index].ArgType != SignalArg::Type::Float) return (0.0f);
  return (Args[Index].AsFloat);
}

template<> inline int32_t SignalArgs::Get<int32_t>(uint8_t Index) const
{
  if (Index >= Count || Args[Index].ArgType != SignalArg::Type::Int) return (0);
  return (Args[Index].AsInt);
}

template<> inline bool SignalArgs::Get<bool>(uint8_t Index) const
{
  if (Index >= Count || Args[Index].ArgType != SignalArg::Type::Bool) return (false);
  return (Args[Index].AsBool);
}

template<> inline std::string_view SignalArgs::Get<std::string_view>(uint8_t Index) const
{
  if (Index >= Count || Args[Index].ArgType != SignalArg::Type::String) return ("");
  return (Args[Index].AsString);
}

template<> inline void* SignalArgs::Get<void*>(uint8_t Index) const
{
  if (Index >= Count || Args[Index].ArgType != SignalArg::Type::Pointer) return (nullptr);
  return (Args[Index].AsPointer);
}

//=============================================================================
// SignalCallback, SignalConnection, SignalSlot
//=============================================================================

using SignalCallback = std::function<void(const SignalArgs&)>;

struct SignalConnection
{
  uint32_t ID;
  SignalCallback Callback;
  Node* Receiver;  // node that owns the callback (for cleanup), may be nullptr
};

struct SignalSlot
{
  std::string Name;
  std::vector<SignalConnection> Connections;
};

//=============================================================================
// OutgoingConnection — tracked on the receiver node for cleanup
//=============================================================================

struct OutgoingConnection
{
  Node* Emitter;
  std::string SignalName;
  uint32_t ConnectionID;
};
