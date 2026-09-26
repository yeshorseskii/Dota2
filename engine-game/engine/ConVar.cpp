#include "ConVar.hpp"
#include <sstream>
#include <algorithm>

namespace eng {

namespace {
// Function-local statics so registration during static init is safe.
std::unordered_map<std::string, ConVar*>& Vars() {
    static std::unordered_map<std::string, ConVar*> m;
    return m;
}
std::unordered_map<std::string, ConCommand*>& Cmds() {
    static std::unordered_map<std::string, ConCommand*> m;
    return m;
}
std::vector<std::string>& LogBuf() {
    static std::vector<std::string> l;
    return l;
}
} // namespace

ConVar::ConVar(const char* name, float defValue, const char* help)
    : name_(name), help_(help), value_(defValue) {
    Con::Register(this);
}

ConCommand::ConCommand(const char* name, Fn fn, const char* help)
    : name_(name), help_(help), fn_(std::move(fn)) {
    Con::Register(this);
}

namespace Con {

void Register(ConVar* v) { Vars()[v->Name()] = v; }
void Register(ConCommand* c) { Cmds()[c->Name()] = c; }

ConVar* FindVar(const std::string& name) {
    auto it = Vars().find(name);
    return it == Vars().end() ? nullptr : it->second;
}
ConCommand* FindCommand(const std::string& name) {
    auto it = Cmds().find(name);
    return it == Cmds().end() ? nullptr : it->second;
}

void Print(const std::string& line) {
    LogBuf().push_back(line);
    if (LogBuf().size() > 200) LogBuf().erase(LogBuf().begin());
}
const std::vector<std::string>& Log() { return LogBuf(); }

std::vector<std::string> CompletionList() {
    std::vector<std::string> names;
    for (auto& kv : Vars()) names.push_back(kv.first);
    for (auto& kv : Cmds()) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    return names;
}

void Exec(const std::string& line) {
    std::istringstream iss(line);
    std::string tok;
    std::vector<std::string> toks;
    while (iss >> tok) toks.push_back(tok);
    if (toks.empty()) return;

    Print("] " + line);
    const std::string& name = toks[0];

    if (name == "help" || name == "find") {
        for (auto& n : CompletionList()) {
            if (auto* v = FindVar(n))
                Print("  " + n + " = " + std::to_string(v->GetFloat()) +
                      (v->Help().empty() ? "" : "   // " + v->Help()));
            else if (auto* c = FindCommand(n))
                Print("  " + n + (c->Help().empty() ? "" : "   // " + c->Help()));
        }
        return;
    }
    if (auto* cmd = FindCommand(name)) {
        cmd->Invoke(std::vector<std::string>(toks.begin() + 1, toks.end()));
        return;
    }
    if (auto* var = FindVar(name)) {
        if (toks.size() >= 2) {
            try { var->SetFloat(std::stof(toks[1])); } catch (...) {
                Print("  bad value"); return;
            }
            Print("  " + name + " = " + std::to_string(var->GetFloat()));
        } else {
            Print("  " + name + " = " + std::to_string(var->GetFloat()) +
                  (var->Help().empty() ? "" : "   // " + var->Help()));
        }
        return;
    }
    Print("  unknown command: " + name + "  (type 'help')");
}

} // namespace Con
} // namespace eng
