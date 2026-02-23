#include "gtest/gtest.h"
#include "AxLog/AxLog.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxAPIRegistry.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <string_view>
#include <chrono>
#include <thread>

/* ------------------------------------------------------------------ */
/*  Test fixture                                                       */
/*                                                                     */
/*  Initializes the Foundation API registry so that PlatformAPI is     */
/*  available for file I/O and time operations.                        */
/* ------------------------------------------------------------------ */

class AxLogTest : public testing::Test
{
protected:
    void SetUp() override
    {
        AxonInitGlobalAPIRegistry();
        AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

        /* Reset runtime log level to default (TRACE) before each test */
        AxLogSetLevel(AX_LOG_LEVEL_TRACE);

        /* Clean up any leftover test log files */
        AxPlatformError ErrorCode;
        PlatformAPI->FileAPI->DeleteFile("test_output.log", &ErrorCode);
        PlatformAPI->FileAPI->DeleteFile("test_file_output.log", &ErrorCode);
        PlatformAPI->FileAPI->DeleteFile("test_close.log", &ErrorCode);
    }

    void TearDown() override
    {
        /* Ensure file output is closed */
        AxLogCloseFile();

        /* Clean up test log files */
        AxPlatformError ErrorCode;
        PlatformAPI->FileAPI->DeleteFile("test_output.log", &ErrorCode);
        PlatformAPI->FileAPI->DeleteFile("test_file_output.log", &ErrorCode);
        PlatformAPI->FileAPI->DeleteFile("test_close.log", &ErrorCode);

        AxonTermGlobalAPIRegistry();
    }

    /* Helper: reads an entire file into a std::string */
    std::string ReadFileContents(const char *Path)
    {
        AxFile File = PlatformAPI->FileAPI->OpenForRead(Path);
        if (!PlatformAPI->FileAPI->IsValid(File))
        {
            return ("");
        }

        uint64_t Size = PlatformAPI->FileAPI->Size(File);
        if (Size == 0)
        {
            PlatformAPI->FileAPI->Close(File);
            return ("");
        }

        std::string Contents(Size, '\0');
        PlatformAPI->FileAPI->Read(File, Contents.data(), (uint32_t)Size);
        PlatformAPI->FileAPI->Close(File);
        return (Contents);
    }
};

/* ------------------------------------------------------------------ */
/*  Test 1: Each severity level produces output                        */
/*                                                                     */
/*  Verifies that TRACE through FATAL all write to a log file when     */
/*  the runtime level is set to TRACE.                                 */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, EachSeverityLevelProducesOutput)
{
    AxLogSetLevel(AX_LOG_LEVEL_TRACE);
    AxLogOpenFile("test_output.log");

    AX_LOG(TRACE,   "trace message");
    AX_LOG(DEBUG,   "debug message");
    AX_LOG(INFO,    "info message");
    AX_LOG(WARNING, "warning message");
    AX_LOG(ERROR,   "error message");
    AX_LOG(FATAL,   "fatal message");

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");
    EXPECT_NE(Contents.find("trace message"),   std::string::npos) << "TRACE should appear in log";
    EXPECT_NE(Contents.find("debug message"),   std::string::npos) << "DEBUG should appear in log";
    EXPECT_NE(Contents.find("info message"),    std::string::npos) << "INFO should appear in log";
    EXPECT_NE(Contents.find("warning message"), std::string::npos) << "WARNING should appear in log";
    EXPECT_NE(Contents.find("error message"),   std::string::npos) << "ERROR should appear in log";
    EXPECT_NE(Contents.find("fatal message"),   std::string::npos) << "FATAL should appear in log";
}

/* ------------------------------------------------------------------ */
/*  Test 2: Runtime level filtering suppresses messages below threshold */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, RuntimeLevelFilteringSuppressesMessages)
{
    AxLogSetLevel(AX_LOG_LEVEL_WARNING);
    AxLogOpenFile("test_output.log");

    AX_LOG(TRACE,   "should not appear trace");
    AX_LOG(DEBUG,   "should not appear debug");
    AX_LOG(INFO,    "should not appear info");
    AX_LOG(WARNING, "visible warning");
    AX_LOG(ERROR,   "visible error");
    AX_LOG(FATAL,   "visible fatal");

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");
    EXPECT_EQ(Contents.find("should not appear trace"), std::string::npos) << "TRACE should be filtered";
    EXPECT_EQ(Contents.find("should not appear debug"), std::string::npos) << "DEBUG should be filtered";
    EXPECT_EQ(Contents.find("should not appear info"),  std::string::npos) << "INFO should be filtered";
    EXPECT_NE(Contents.find("visible warning"), std::string::npos) << "WARNING should pass filter";
    EXPECT_NE(Contents.find("visible error"),   std::string::npos) << "ERROR should pass filter";
    EXPECT_NE(Contents.find("visible fatal"),   std::string::npos) << "FATAL should pass filter";
}

/* ------------------------------------------------------------------ */
/*  Test 3: AxLogSetLevel changes the active filter                    */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, SetLevelChangesActiveFilter)
{
    EXPECT_EQ(AxLogGetRuntimeLevel(), AX_LOG_LEVEL_TRACE) << "Default should be TRACE";

    AxLogSetLevel(AX_LOG_LEVEL_ERROR);
    EXPECT_EQ(AxLogGetRuntimeLevel(), AX_LOG_LEVEL_ERROR) << "Should update to ERROR";

    AxLogSetLevel(AX_LOG_LEVEL_OFF);
    EXPECT_EQ(AxLogGetRuntimeLevel(), AX_LOG_LEVEL_OFF) << "Should update to OFF";

    AxLogSetLevel(AX_LOG_LEVEL_TRACE);
    EXPECT_EQ(AxLogGetRuntimeLevel(), AX_LOG_LEVEL_TRACE) << "Should reset to TRACE";
}

/* ------------------------------------------------------------------ */
/*  Test 4: Format output contains date/time, filename, line, label    */
/*                                                                     */
/*  Verifies the column-aligned format includes:                       */
/*    [YYYY-MM-DD HH:MM:SS.mmm] [+uptime] [tid:XXXX] file:line LEVEL */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, FormatOutputContainsExpectedFields)
{
    AxLogOpenFile("test_output.log");

    AX_LOG(INFO, "format test message");

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");

    /* Check date/time pattern: [YYYY-MM-DD HH:MM:SS.mmm] */
    EXPECT_NE(Contents.find("[20"), std::string::npos)
        << "Output should contain date starting with [20";

    /* Check filename (not full path) -- this file is AxLogTests.cpp */
    EXPECT_NE(Contents.find("AxLogTests.cpp"), std::string::npos)
        << "Output should contain filename without full path";

    /* Check that full path is NOT present (should be stripped) */
    EXPECT_EQ(Contents.find("Plugins/AxLog/tests/src/AxLogTests.cpp"), std::string::npos)
        << "Output should NOT contain full path";

    /* Check line number appears (colon after filename) */
    EXPECT_NE(Contents.find("AxLogTests.cpp:"), std::string::npos)
        << "Output should contain filename:line format";

    /* Check severity label */
    EXPECT_NE(Contents.find("INFO"), std::string::npos)
        << "Output should contain severity label";

    /* Check uptime field */
    EXPECT_NE(Contents.find("[+"), std::string::npos)
        << "Output should contain uptime field";

    /* Check thread ID field */
    EXPECT_NE(Contents.find("[tid:"), std::string::npos)
        << "Output should contain thread ID field";

    /* Check user message */
    EXPECT_NE(Contents.find("format test message"), std::string::npos)
        << "Output should contain the user message";
}

/* ------------------------------------------------------------------ */
/*  Test 5: Compile-time stripping does not evaluate arguments         */
/*                                                                     */
/*  When AX_LOG_COMPILE_LEVEL is set above a level, the macro should   */
/*  expand to a no-op and never evaluate its arguments. Since we       */
/*  cannot change AX_LOG_COMPILE_LEVEL at runtime, we verify the       */
/*  default allows all levels (Test 6), and here we verify the macro   */
/*  structure by confirming that runtime filtering also prevents       */
/*  argument evaluation (the same if-guard mechanism).                 */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, CompileTimeStrippingDoesNotEvaluateArguments)
{
    /* With the default AX_LOG_COMPILE_LEVEL = AX_LOG_LEVEL_TRACE, all
     * levels pass the compile-time check. We verify the lazy evaluation
     * mechanism by setting the runtime level high and confirming that
     * a side-effecting argument is not evaluated. */
    int Counter = 0;

    AxLogSetLevel(AX_LOG_LEVEL_OFF);

    /* If the macro evaluates its arguments even when filtered, Counter
     * would be incremented. The if-guard should prevent evaluation. */
    AX_LOG(TRACE, "value = %d", ++Counter);
    AX_LOG(DEBUG, "value = %d", ++Counter);
    AX_LOG(INFO,  "value = %d", ++Counter);

    EXPECT_EQ(Counter, 0)
        << "Arguments should not be evaluated when log level is filtered out";
}

/* ------------------------------------------------------------------ */
/*  Test 6: Default compile level allows all levels through            */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, DefaultCompileLevelAllowsAllLevels)
{
    /* AX_LOG_COMPILE_LEVEL defaults to AX_LOG_LEVEL_TRACE (0), so all
     * severity levels should pass the compile-time check. We verify
     * by ensuring every level can produce output. */
    EXPECT_EQ(AX_LOG_COMPILE_LEVEL, AX_LOG_LEVEL_TRACE)
        << "Default compile level should be TRACE";

    AxLogSetLevel(AX_LOG_LEVEL_TRACE);
    AxLogOpenFile("test_output.log");

    AX_LOG(TRACE,   "compile-check-trace");
    AX_LOG(DEBUG,   "compile-check-debug");
    AX_LOG(INFO,    "compile-check-info");
    AX_LOG(WARNING, "compile-check-warning");
    AX_LOG(ERROR,   "compile-check-error");
    AX_LOG(FATAL,   "compile-check-fatal");

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");
    EXPECT_NE(Contents.find("compile-check-trace"),   std::string::npos);
    EXPECT_NE(Contents.find("compile-check-debug"),   std::string::npos);
    EXPECT_NE(Contents.find("compile-check-info"),    std::string::npos);
    EXPECT_NE(Contents.find("compile-check-warning"), std::string::npos);
    EXPECT_NE(Contents.find("compile-check-error"),   std::string::npos);
    EXPECT_NE(Contents.find("compile-check-fatal"),   std::string::npos);
}

/* ------------------------------------------------------------------ */
/*  Test 7: AxLogOpenFile creates a log file with plain-text output    */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, OpenFileCreatesLogFileWithPlainTextOutput)
{
    AxLogOpenFile("test_file_output.log");

    AX_LOG(INFO, "file output test");

    AxLogCloseFile();

    /* Verify the file exists and has content */
    EXPECT_TRUE(PlatformAPI->PathAPI->FileExists("test_file_output.log"))
        << "Log file should be created by AxLogOpenFile";

    std::string Contents = ReadFileContents("test_file_output.log");
    EXPECT_FALSE(Contents.empty()) << "Log file should not be empty";
    EXPECT_NE(Contents.find("file output test"), std::string::npos)
        << "Log file should contain the logged message";

    /* Verify no ANSI escape codes in file output */
    EXPECT_EQ(Contents.find("\x1b["), std::string::npos)
        << "File output should NOT contain ANSI escape codes";

    /* Verify column-aligned format fields are present */
    EXPECT_NE(Contents.find("[20"), std::string::npos)
        << "File output should contain date/time";
    EXPECT_NE(Contents.find("[+"), std::string::npos)
        << "File output should contain uptime";
    EXPECT_NE(Contents.find("[tid:"), std::string::npos)
        << "File output should contain thread ID";
    EXPECT_NE(Contents.find("INFO"), std::string::npos)
        << "File output should contain severity label";
}

/* ------------------------------------------------------------------ */
/*  Test 8: AxLogCloseFile stops file output                           */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, CloseFileStopsFileOutput)
{
    AxLogOpenFile("test_close.log");

    AX_LOG(INFO, "before close");

    AxLogCloseFile();

    /* After closing, further log calls should NOT write to the file */
    AX_LOG(INFO, "after close");

    std::string Contents = ReadFileContents("test_close.log");
    EXPECT_NE(Contents.find("before close"), std::string::npos)
        << "Messages before close should be in the file";
    EXPECT_EQ(Contents.find("after close"), std::string::npos)
        << "Messages after close should NOT be in the file";
}

/* ------------------------------------------------------------------ */
/*  Test 9: AX_LOG_IF only produces output when condition is true      */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, LogIfOnlyOutputsWhenConditionTrue)
{
    AxLogOpenFile("test_output.log");

    AX_LOG_IF(WARNING, true,  "condition-true message");
    AX_LOG_IF(WARNING, false, "condition-false message");

    int Value = 42;
    AX_LOG_IF(INFO, Value > 10, "value-above-threshold");
    AX_LOG_IF(INFO, Value < 10, "value-below-threshold");

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");
    EXPECT_NE(Contents.find("condition-true message"), std::string::npos)
        << "AX_LOG_IF should output when condition is true";
    EXPECT_EQ(Contents.find("condition-false message"), std::string::npos)
        << "AX_LOG_IF should NOT output when condition is false";
    EXPECT_NE(Contents.find("value-above-threshold"), std::string::npos)
        << "AX_LOG_IF should output when runtime condition is true";
    EXPECT_EQ(Contents.find("value-below-threshold"), std::string::npos)
        << "AX_LOG_IF should NOT output when runtime condition is false";
}

/* ------------------------------------------------------------------ */
/*  Test 10: Variadic format arguments are interpolated in output      */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, VariadicFormatArgumentsAppearInOutput)
{
    AxLogOpenFile("test_output.log");

    /* String argument */
    AX_LOG(INFO, "Loaded shader: %s", "phong.glsl");

    /* Integer argument */
    AX_LOG(DEBUG, "Player health: %d", 85);

    /* Multiple arguments */
    AX_LOG(WARNING, "Position: (%.1f, %.1f, %.1f)", 1.5f, 2.0f, -3.7f);

    /* No variadic arguments (just a plain string) */
    AX_LOG(ERROR, "Simple message with no args");

    /* AX_LOG_IF with variadic arguments */
    int Health = 15;
    AX_LOG_IF(WARNING, Health < 20, "Health critical: %d", Health);

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");

    /* Verify interpolated values appear in the output */
    EXPECT_NE(Contents.find("Loaded shader: phong.glsl"), std::string::npos)
        << "String argument should be interpolated";
    EXPECT_NE(Contents.find("Player health: 85"), std::string::npos)
        << "Integer argument should be interpolated";
    EXPECT_NE(Contents.find("Position: (1.5, 2.0, -3.7)"), std::string::npos)
        << "Multiple float arguments should be interpolated";
    EXPECT_NE(Contents.find("Simple message with no args"), std::string::npos)
        << "Plain string with no variadic args should work";
    EXPECT_NE(Contents.find("Health critical: 15"), std::string::npos)
        << "AX_LOG_IF with variadic args should interpolate correctly";
}

/* ------------------------------------------------------------------ */
/*  Test 11: AX_LOG_IF does not evaluate args when condition is false  */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, LogIfDoesNotEvaluateArgsWhenConditionFalse)
{
    int Counter = 0;

    /* Condition is false -- the format arguments should NOT be evaluated */
    AX_LOG_IF(INFO, false, "counter = %d", ++Counter);
    EXPECT_EQ(Counter, 0)
        << "AX_LOG_IF should not evaluate arguments when condition is false";

    /* Also verify that runtime level filtering prevents evaluation in AX_LOG_IF */
    AxLogSetLevel(AX_LOG_LEVEL_OFF);
    AX_LOG_IF(INFO, true, "counter = %d", ++Counter);
    EXPECT_EQ(Counter, 0)
        << "AX_LOG_IF should not evaluate arguments when level is filtered by runtime level";
}

/* ------------------------------------------------------------------ */
/*  Test 12: AX_LOG works when called from a C translation unit        */
/*                                                                     */
/*  Verifies that the macros compile and produce correct output when   */
/*  included from a .c file (C-linkage compatibility, especially       */
/*  important given the Windows ERROR macro conflict fix).             */
/* ------------------------------------------------------------------ */

extern "C" void AxLogCTestFunction(void);

TEST_F(AxLogTest, MacroWorksFromCTranslationUnit)
{
    AxLogOpenFile("test_output.log");

    AxLogCTestFunction();

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");

    /* Verify messages from the C function appear in the log */
    EXPECT_NE(Contents.find("message from C translation unit"), std::string::npos)
        << "AX_LOG(INFO, ...) should work from a C file";
    EXPECT_NE(Contents.find("error from C: code 42"), std::string::npos)
        << "AX_LOG(ERROR, ...) with variadic args should work from a C file";
    EXPECT_NE(Contents.find("conditional from C: active"), std::string::npos)
        << "AX_LOG_IF with true condition should work from a C file";
    EXPECT_EQ(Contents.find("this should not appear from C"), std::string::npos)
        << "AX_LOG_IF with false condition should not produce output from a C file";

    /* Verify correct severity labels appear */
    EXPECT_NE(Contents.find("INFO"), std::string::npos);
    EXPECT_NE(Contents.find("ERROR"), std::string::npos);
    EXPECT_NE(Contents.find("WARNING"), std::string::npos);

    /* Verify the C source filename appears (not a C++ file) */
    EXPECT_NE(Contents.find("AxLogCTest.c:"), std::string::npos)
        << "Output should show the C source filename";
}

/* ------------------------------------------------------------------ */
/*  Test 13: Column alignment is consistent across severity levels     */
/*                                                                     */
/*  Verifies that the severity label column starts at the same         */
/*  position on every log line, regardless of severity label length.   */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, ColumnAlignmentIsConsistentAcrossLines)
{
    AxLogOpenFile("test_output.log");

    AX_LOG(TRACE,   "alignment-test-trace");
    AX_LOG(DEBUG,   "alignment-test-debug");
    AX_LOG(INFO,    "alignment-test-info");
    AX_LOG(WARNING, "alignment-test-warning");
    AX_LOG(ERROR,   "alignment-test-error");
    AX_LOG(FATAL,   "alignment-test-fatal");

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");

    const char *Labels[]  = { "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    const char *Markers[] = { "alignment-test-trace", "alignment-test-debug",
                              "alignment-test-info",  "alignment-test-warning",
                              "alignment-test-error", "alignment-test-fatal" };

    size_t FirstLabelCol = std::string::npos;

    for (int i = 0; i < 6; i++)
    {
        auto MarkerPos = Contents.find(Markers[i]);
        ASSERT_NE(MarkerPos, std::string::npos)
            << "Should find marker: " << Markers[i];

        /* Find the start of this line */
        auto LineStart = Contents.rfind('\n', MarkerPos);
        LineStart = (LineStart == std::string::npos) ? 0 : LineStart + 1;

        /* Find the severity label on this line (before the marker) */
        auto LabelPos = Contents.find(Labels[i], LineStart);
        ASSERT_NE(LabelPos, std::string::npos)
            << "Should find label: " << Labels[i];
        ASSERT_LT(LabelPos, MarkerPos)
            << "Label should appear before the marker text";

        size_t Col = LabelPos - LineStart;

        if (FirstLabelCol == std::string::npos)
        {
            FirstLabelCol = Col;
        }
        else
        {
            EXPECT_EQ(Col, FirstLabelCol)
                << "Severity label '" << Labels[i] << "' at column " << Col
                << " should match first label column " << FirstLabelCol;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Test 14: Uptime increases between successive log calls             */
/*                                                                     */
/*  Verifies that the [+N.NNNs] uptime field increases over time,     */
/*  confirming that the Platform TimeAPI wall clock is used correctly. */
/* ------------------------------------------------------------------ */

TEST_F(AxLogTest, UptimeIncreasesBetweenSuccessiveCalls)
{
    AxLogOpenFile("test_output.log");

    AX_LOG(INFO, "first-uptime-marker");

    /* Sleep briefly to ensure measurable uptime difference */
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    AX_LOG(INFO, "second-uptime-marker");

    AxLogCloseFile();

    std::string Contents = ReadFileContents("test_output.log");

    /* Locate each marker in the file */
    auto Pos1 = Contents.find("first-uptime-marker");
    auto Pos2 = Contents.find("second-uptime-marker");
    ASSERT_NE(Pos1, std::string::npos);
    ASSERT_NE(Pos2, std::string::npos);

    /* Find the start of each marker's line */
    auto LineStart1 = Contents.rfind('\n', Pos1);
    LineStart1 = (LineStart1 == std::string::npos) ? 0 : LineStart1 + 1;

    auto LineStart2 = Contents.rfind('\n', Pos2);
    LineStart2 = (LineStart2 == std::string::npos) ? 0 : LineStart2 + 1;

    /* Find the [+ uptime field on each line */
    auto UptimePos1 = Contents.find("[+", LineStart1);
    auto UptimePos2 = Contents.find("[+", LineStart2);
    ASSERT_NE(UptimePos1, std::string::npos);
    ASSERT_NE(UptimePos2, std::string::npos);
    ASSERT_LT(UptimePos1, Pos1) << "Uptime field should be before the marker";
    ASSERT_LT(UptimePos2, Pos2) << "Uptime field should be before the marker";

    /* Parse the uptime float values (format: [+N.NNNs]) */
    float Uptime1 = std::stof(Contents.substr(UptimePos1 + 2));
    float Uptime2 = std::stof(Contents.substr(UptimePos2 + 2));

    EXPECT_GT(Uptime2, Uptime1)
        << "Second log call uptime (" << Uptime2 << "s) should be greater than first ("
        << Uptime1 << "s)";
}
