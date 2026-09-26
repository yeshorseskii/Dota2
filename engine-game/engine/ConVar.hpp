// Console variables and commands — a small take on Source's cvar system.
//
//   static eng::ConVar sv_wave_interval("sv_wave_interval", 22.f, "seconds between waves");
//   static eng::ConCommand restart("restart", [](auto& a){ ... }, "restart the match");
//
// Values are settable at runtime from the in-game console (`~`).
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace eng {

class ConVar {
public:
    ConVar(const char* name, float defValue, const char* help = "");
    float GetFloat() const { return value_; }
    int   GetInt() const { return static_cast<int>(value_); }
    bool  GetBool() const { return value_ != 0.0f; }
    void  SetFloat(float v) { value_ = v; }
    const std::string& Name() const { return name_; }
    const std::string& Help() const { return help_; }

private:
    std::string name_, help_;
    float value_;
};

class ConCommand {
public:
    using Args = std::vector<std::string>;
    using Fn = std::function<void(const Args&)>;
    ConCommand(const char* name, Fn fn, const char* help = "");
    void Invoke(const Args& a) const { fn_(a); }
    const std::string& Name() const { return name_; }
    const std::string& Help() const { return help_; }

private:
    std::string name_, help_;
    Fn fn_;
};

// Global console: registry + log buffer + command execution.
namespace Con {
void Register(ConVar* v);
void Register(ConCommand* c);
ConVar* FindVar(const std::string& name);
ConCommand* FindCommand(const std::string& name);

void Exec(const std::string& line);           // parse & dispatch one line
void Print(const std::string& line);          // append to the log
const std::vector<std::string>& Log();
std::vector<std::string> CompletionList();     // all names, for help
} // namespace Con

} // namespace eng
