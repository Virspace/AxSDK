/**
 * AxScriptLog.cpp - Structured log buffer implementation
 *
 * LogBuffer stores entries in a fixed-size ring buffer. Log:: namespace
 * functions push entries with empty NodeName for engine-internal warnings.
 */

#include "AxEngine/AxScriptLog.h"

//=============================================================================
// LogBuffer
//=============================================================================

LogBuffer& LogBuffer::Get()
{
  static LogBuffer sInstance;
  return (sInstance);
}

void LogBuffer::Push(int Level, std::string_view NodeName, std::string_view Message)
{
  LogEntry& Entry = Entries_[Head_];
  Entry.Level = Level;
  Entry.NodeName = std::string(NodeName);
  Entry.Message = std::string(Message);

  Head_ = (Head_ + 1) % kMaxEntries;
  if (Count_ < kMaxEntries) {
    Count_++;
  }
}

const LogEntry& LogBuffer::GetEntry(uint32_t Index) const
{
  // Index 0 = oldest entry
  uint32_t Start = (Count_ < kMaxEntries) ? 0 : Head_;
  uint32_t Actual = (Start + Index) % kMaxEntries;
  return (Entries_[Actual]);
}

void LogBuffer::Clear()
{
  Head_ = 0;
  Count_ = 0;
}

//=============================================================================
// Log Namespace
//=============================================================================

namespace Log {

void Error(std::string_view Message)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_ERROR, "", Message);
}

void Warn(std::string_view Message)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_WARNING, "", Message);
}

void Info(std::string_view Message)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_INFO, "", Message);
}

void Debug(std::string_view Message)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_DEBUG, "", Message);
}

} // namespace Log
