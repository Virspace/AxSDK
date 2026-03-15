#pragma once

/**
 * AxScriptLog.h - Structured log buffer for scripts and the editor
 *
 * LogBuffer is a fixed-size ring buffer of LogEntry structs. Scripts push
 * entries via ScriptBase::LogWarn etc. (which include the owner node name),
 * and engine failure paths push via the free-standing Log:: namespace
 * (with empty node name). The editor reads entries each frame for its
 * output panel.
 *
 * This is separate from AxLog, which is engine-internal logging.
 */

#include <string>
#include <string_view>
#include <cstdint>

//=============================================================================
// Log Levels (reuse AxLog level constants)
//=============================================================================

#ifndef AX_LOG_LEVEL_TRACE
#define AX_LOG_LEVEL_TRACE   0
#define AX_LOG_LEVEL_DEBUG   1
#define AX_LOG_LEVEL_INFO    2
#define AX_LOG_LEVEL_WARNING 3
#define AX_LOG_LEVEL_ERROR   4
#define AX_LOG_LEVEL_FATAL   5
#endif

//=============================================================================
// LogEntry
//=============================================================================

struct LogEntry
{
  int Level = AX_LOG_LEVEL_INFO;
  std::string NodeName;
  std::string Message;
};

//=============================================================================
// LogBuffer
//=============================================================================

class LogBuffer
{
public:
  static constexpr uint32_t kMaxEntries = 1024;

  static LogBuffer& Get();

  void Push(int Level, std::string_view NodeName, std::string_view Message);

  uint32_t GetCount() const { return (Count_); }
  const LogEntry& GetEntry(uint32_t Index) const;
  void Clear();

private:
  LogBuffer() = default;

  LogEntry Entries_[kMaxEntries];
  uint32_t Head_ = 0;
  uint32_t Count_ = 0;
};

//=============================================================================
// Log Namespace (free-standing, for engine-internal use)
//=============================================================================

namespace Log {

void Error(std::string_view Message);
void Warn(std::string_view Message);
void Info(std::string_view Message);
void Debug(std::string_view Message);

} // namespace Log
