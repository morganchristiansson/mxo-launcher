// Recovered scaffold for launcher.exe console variable parsing / typed console vars.
// Original source not available.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace mxo {
namespace libltbase {

struct ConsoleParseErrorSink {
    std::vector<std::string> lines;
};

struct ConsoleConfigParseState {
    FILE* primaryFile = nullptr;
    FILE* activeFile = nullptr;
    std::vector<FILE*> includeStack;

    std::string configFilePath = "autoexec.cfg";
    std::string configSectionName;
    std::string extrasSectionsCsv;

    std::string workingLine;
    std::string currentName;
    std::string currentValue;

    bool foundSectionHeader = false;
    bool reachedEndOfFile = false;
    bool ignoreUnknownVars = true;
    bool parseConfigFileEnabled = true;
};

class CConsoleVar {
public:
    CConsoleVar() = default;
    virtual ~CConsoleVar();

    virtual bool ParseValue(const char* valueText) = 0;
    virtual int FormatValue(char* destination, int destinationCapacity) const = 0;
    virtual void Dump() const = 0;

    const char* Name() const;
    void SetRecoveredName(const char* name);
    bool InitializedFromExternalSource() const;
    void SetInitializedFromExternalSource(bool initialized);
    void RegisterSelf();
    void UnregisterSelf();

    // Recovered common field surface from the typed console-var leaves:
    //   +0x04 = variable name pointer used by Dump implementations
    //   +0x1c = constructor-supplied flags
    //   +0x20/+0x24/+0x28 = optional validate/notify callbacks
    //   +0x2d = "initialized from external source" byte set by command-line/config parsing
    static void ReportParseError(ConsoleParseErrorSink* errors, const char* format, ...);
    static bool ParseCommandLine(std::uint32_t argc, char** argv, ConsoleParseErrorSink* errors);
    static char* GetNextConfigFileLine(
        ConsoleConfigParseState& state,
        char* lineBuffer,
        int lineBufferSize,
        ConsoleParseErrorSink* errors);
    static bool ParseConfigFileSectionLine(ConsoleConfigParseState& state, ConsoleParseErrorSink* errors);
    static bool FindConfigFileSection(
        ConsoleConfigParseState& state,
        const char* sectionName,
        ConsoleParseErrorSink* errors);
    static bool ParseConfigFileSection(
        ConsoleConfigParseState& state,
        void* pendingNotifications,
        ConsoleParseErrorSink* errors);
    static bool ParseConfigFile(ConsoleConfigParseState& state, ConsoleParseErrorSink* errors);
    static bool ParseCommandLineAndConfig(std::uint32_t argc, char** argv, ConsoleParseErrorSink* errors);

    // Config state management helpers (for custom parsers)
    static void CloseConfigState(ConsoleConfigParseState& state);
    static void RewindConfigState(ConsoleConfigParseState& state);

protected:
    // Scaffold-only storage for future reimplementation work.
    // This is not claimed as exact original memory layout.
    std::string name_;
    std::string description_;
    std::uint32_t flags_ = 0;
    bool initializedFromExternalSource_ = false;
};

class CConsoleInt : public CConsoleVar {
public:
    CConsoleInt();

    // Recovered leaf field surface:
    //   +0x34 = integer value
    //   +0x38 = "display as hex" byte
    bool ParseValue(const char* valueText) override;
    int FormatValue(char* destination, int destinationCapacity) const override;
    void Dump() const override;

private:
    std::int32_t value_ = 0;
    bool formatAsHex_ = false;
};

class CConsoleString : public CConsoleVar {
public:
    CConsoleString();
    CConsoleString(const char* name, const char* initialValue, const char* helpText, std::uint32_t flags);
    ~CConsoleString() override;

    // Recovered leaf field surface:
    //   +0x30 = default string buffer
    //   +0x34 = live string buffer
    //   +0x38 = allocation size
    //   +0x3c = live string length
    bool ParseValue(const char* valueText) override;
    int FormatValue(char* destination, int destinationCapacity) const override;
    void Dump() const override;

    void AssignValue(const char* valueText);
    void DestroyBuffers();

private:
    std::string defaultValue_;
    std::string value_;
};

class CConsoleBool : public CConsoleVar {
public:
    CConsoleBool();

    // Recovered leaf field surface:
    //   +0x31 = boolean value byte
    bool ParseValue(const char* valueText) override;
    int FormatValue(char* destination, int destinationCapacity) const override;
    void Dump() const override;

private:
    bool value_ = false;
};

class CConsoleFloat : public CConsoleVar {
public:
    CConsoleFloat();

    // Recovered leaf field surface:
    //   +0x34 = float value
    bool ParseValue(const char* valueText) override;
    int FormatValue(char* destination, int destinationCapacity) const override;
    void Dump() const override;

private:
    float value_ = 0.0f;
};

} // namespace libltbase
} // namespace mxo
