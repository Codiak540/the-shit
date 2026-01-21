#include <dirent.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <regex>
#include <memory>
#include <cstdlib>
#include <set>
#include <sys/wait.h>
#include <sys/stat.h>

// Utility functions
namespace utils {
    std::string to_lower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    bool contains(const std::string& str, const std::string& substr) {
        return str.find(substr) != std::string::npos;
    }

    bool starts_with(const std::string& str, const std::string& prefix) {
        return str.find(prefix) == 0;
    }

    bool ends_with(const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    std::vector<std::string> split(const std::string& str, char delim = ' ') {
        std::vector<std::string> parts;
        std::istringstream iss(str);
        std::string part;
        while (std::getline(iss, part, delim)) {
            if (!part.empty()) parts.push_back(part);
        }
        return parts;
    }

    bool file_exists(const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

    bool is_directory(const std::string& path) {
        struct stat buffer;
        return stat(path.c_str(), &buffer) == 0 && S_ISDIR(buffer.st_mode);
    }
}

// Command structure
struct Command {
    std::string script;
    std::string output;
    std::vector<std::string> script_parts;

    Command(const std::string& s, const std::string& o) : script(s), output(o) {
        script_parts = utils::split(s);
    }
};

// Base Rule class
class Rule {
public:
    virtual ~Rule() = default;
    virtual bool match(const Command& cmd) const = 0;
    virtual std::vector<std::string> get_new_command(const Command& cmd) const = 0;
    virtual std::string get_name() const = 0;
    virtual int get_priority() const { return 1000; }
    virtual bool is_enabled_by_default() const { return true; }
    virtual bool requires_output() const { return false; }
};

// Macro to simplify rule definitions
#define RULE_CLASS(name) class name : public Rule { \
public: \
    std::string get_name() const override { return #name; } \
    bool match(const Command& cmd) const override; \
    std::vector<std::string> get_new_command(const Command& cmd) const override; \
}

RULE_CLASS(SudoRule);
bool SudoRule::match(const Command& cmd) const {
    std::string lower = utils::to_lower(cmd.output);
    return utils::contains(lower, "permission denied") ||
        utils::contains(lower, "Permission denied") ||
           utils::contains(lower, "EACCES") ||
               utils::contains(cmd.output, "unless you are root");
}
std::vector<std::string> SudoRule::get_new_command(const Command& cmd) const {
    return {"sudo " + cmd.script};
}

RULE_CLASS(GitPushRule);
bool GitPushRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git push") &&
           utils::contains(cmd.output, "has no upstream branch");
}
std::vector<std::string> GitPushRule::get_new_command(const Command& cmd) const {
    std::regex branch_regex("git push --set-upstream origin ([a-zA-Z0-9_-]+)");
    std::smatch match;
    if (std::regex_search(cmd.output, match, branch_regex) && match.size() > 1) {
        return {"git push --set-upstream origin " + match[1].str()};
    }
    return {"git push --set-upstream origin master"};
}

RULE_CLASS(NoCommandRule);
bool NoCommandRule::match(const Command& cmd) const {
    return utils::contains(cmd.output, "command not found") ||
           utils::contains(cmd.output, "No command");
}
std::vector<std::string> NoCommandRule::get_new_command(const Command& cmd) const {
    std::map<std::string, std::string> typos = {
        {"puthon", "python"}, {"pytohn", "python"}, {"gti", "git"},
        {"vom", "vim"}, {"claer", "clear"}, {"cd..", "cd .."},
        {"sl", "ls"}, {"grpe", "grep"}, {"pyton", "python"}
    };
    if (!cmd.script_parts.empty() && typos.count(cmd.script_parts[0])) {
        std::string fixed = typos[cmd.script_parts[0]];
        for (size_t i = 1; i < cmd.script_parts.size(); i++) {
            fixed += " " + cmd.script_parts[i];
        }
        return {fixed};
    }
    return {cmd.script};
}

RULE_CLASS(GitNotCommandRule);
bool GitNotCommandRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git") &&
           utils::contains(cmd.output, "is not a git command");
}
std::vector<std::string> GitNotCommandRule::get_new_command(const Command& cmd) const {
    std::regex did_you_mean("The most similar command is\\s+([a-z]+)");
    std::smatch match;
    if (std::regex_search(cmd.output, match, did_you_mean) && match.size() > 1) {
        std::string fixed = "git " + match[1].str();
        for (size_t i = 2; i < cmd.script_parts.size(); i++) {
            fixed += " " + cmd.script_parts[i];
        }
        return {fixed};
    }
    return {cmd.script};
}

RULE_CLASS(GitNotRepositoryRule);
bool GitNotRepositoryRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git") &&
           utils::contains(cmd.output, "fatal: not a git repository (or any of the parent directories):");
}
std::vector<std::string> GitNotRepositoryRule::get_new_command(const Command& cmd) const {
    return {"git create"};
}

RULE_CLASS(CdMkdirRule);
bool CdMkdirRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "cd ") &&
           (utils::contains(cmd.output, "No such file or directory") ||
            utils::contains(cmd.output, "cannot access"));
}
std::vector<std::string> CdMkdirRule::get_new_command(const Command& cmd) const {
    if (cmd.script_parts.size() >= 2) {
        return {"mkdir -p " + cmd.script_parts[1] + " && cd " + cmd.script_parts[1]};
    }
    return {cmd.script};
}

RULE_CLASS(CdParentRule);
bool CdParentRule::match(const Command& cmd) const {
    return cmd.script == "cd..";
}
std::vector<std::string> CdParentRule::get_new_command(const Command& cmd) const {
    return {"cd .."};
}

RULE_CLASS(CdCsRule);
bool CdCsRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "cs ");
}
std::vector<std::string> CdCsRule::get_new_command(const Command& cmd) const {
    return {"cd " + cmd.script.substr(3)};
}

RULE_CLASS(CatDirRule);
bool CatDirRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "cat ") &&
           (utils::contains(cmd.output, "Is a directory") ||
            utils::contains(cmd.output, "is a directory"));
}
std::vector<std::string> CatDirRule::get_new_command(const Command& cmd) const {
    return {"ls " + cmd.script.substr(4)};
}

RULE_CLASS(ChmodXRule);
bool ChmodXRule::match(const Command& cmd) const {
    return utils::contains(cmd.output, "Permission denied") &&
           cmd.script_parts.size() >= 1 &&
           cmd.script_parts[0][0] == '.' && cmd.script_parts[0][1] == '/';
}
std::vector<std::string> ChmodXRule::get_new_command(const Command& cmd) const {
    return {"chmod +x " + cmd.script_parts[0] + " && " + cmd.script};
}

RULE_CLASS(CpOmittingDirectoryRule);
bool CpOmittingDirectoryRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "cp ") &&
           utils::contains(cmd.output, "omitting directory");
}
std::vector<std::string> CpOmittingDirectoryRule::get_new_command(const Command& cmd) const {
    return {"cp -r " + cmd.script.substr(3)};
}

RULE_CLASS(DryRule);
bool DryRule::match(const Command& cmd) const {
    if (cmd.script_parts.size() < 2) return false;
    return cmd.script_parts[0] == cmd.script_parts[1];
}
std::vector<std::string> DryRule::get_new_command(const Command& cmd) const {
    std::string fixed = cmd.script_parts[0];
    for (size_t i = 2; i < cmd.script_parts.size(); i++) {
        fixed += " " + cmd.script_parts[i];
    }
    return {fixed};
}

RULE_CLASS(GitAddRule);
bool GitAddRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git add") &&
           utils::contains(cmd.output, "did not match any file");
}
std::vector<std::string> GitAddRule::get_new_command(const Command& cmd) const {
    return {"git add -A"};
}

RULE_CLASS(GitAddForceRule);
bool GitAddForceRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git add") &&
           (utils::contains(cmd.output, ".gitignore") ||
            utils::contains(cmd.output, "ignored"));
}
std::vector<std::string> GitAddForceRule::get_new_command(const Command& cmd) const {
    return {cmd.script + " --force"};
}

RULE_CLASS(GitBranchDeleteRule);
bool GitBranchDeleteRule::match(const Command& cmd) const {
    return utils::contains(cmd.script, "git branch -d") &&
           utils::contains(cmd.output, "not fully merged");
}
std::vector<std::string> GitBranchDeleteRule::get_new_command(const Command& cmd) const {
    return {cmd.script.substr(0, cmd.script.find("-d")) + "-D" + cmd.script.substr(cmd.script.find("-d") + 2)};
}

RULE_CLASS(GitCommitAddRule);
bool GitCommitAddRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git commit") &&
           utils::contains(cmd.output, "no changes added to commit");
}
std::vector<std::string> GitCommitAddRule::get_new_command(const Command& cmd) const {
    std::string base = cmd.script.substr(0, 10);
    std::string rest = cmd.script.length() > 10 ? cmd.script.substr(10) : "";
    return {"git commit -a" + rest, "git commit -p" + rest};
}

RULE_CLASS(GitCommitAmendRule);
bool GitCommitAmendRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git commit") &&
           !utils::contains(cmd.script, "--amend");
}
std::vector<std::string> GitCommitAmendRule::get_new_command(const Command& cmd) const {
    return {cmd.script + " --amend"};
}

RULE_CLASS(GitPullRule);
bool GitPullRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git pull") &&
           utils::contains(cmd.output, "no tracking information");
}
std::vector<std::string> GitPullRule::get_new_command(const Command& cmd) const {
    return {"git branch --set-upstream-to=origin/master master && git pull"};
}

RULE_CLASS(GitTwoDashesRule);
bool GitTwoDashesRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git ") &&
           (utils::contains(cmd.script, " -amend") ||
            utils::contains(cmd.script, " -continue") ||
            utils::contains(cmd.script, " -abort"));
}
std::vector<std::string> GitTwoDashesRule::get_new_command(const Command& cmd) const {
    std::string fixed = cmd.script;
    size_t pos;
    if ((pos = fixed.find(" -amend")) != std::string::npos) {
        fixed.replace(pos, 7, " --amend");
    } else if ((pos = fixed.find(" -continue")) != std::string::npos) {
        fixed.replace(pos, 10, " --continue");
    } else if ((pos = fixed.find(" -abort")) != std::string::npos) {
        fixed.replace(pos, 7, " --abort");
    }
    return {fixed};
}

RULE_CLASS(GrepRecursiveRule);
bool GrepRecursiveRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "grep ") &&
           utils::contains(cmd.output, "Is a directory");
}
std::vector<std::string> GrepRecursiveRule::get_new_command(const Command& cmd) const {
    return {"grep -r " + cmd.script.substr(5)};
}

RULE_CLASS(HasExistsScriptRule);
bool HasExistsScriptRule::match(const Command& cmd) const {
    return utils::contains(cmd.output, "command not found") &&
           !cmd.script_parts.empty() &&
           utils::file_exists(cmd.script_parts[0]);
}
std::vector<std::string> HasExistsScriptRule::get_new_command(const Command& cmd) const {
    return {"./" + cmd.script};
}

RULE_CLASS(LsAllRule);
bool LsAllRule::match(const Command& cmd) const {
    return cmd.script == "ls" && cmd.output.empty();
}
std::vector<std::string> LsAllRule::get_new_command(const Command& cmd) const {
    return {"ls -A"};
}

RULE_CLASS(LsLahRule);
bool LsLahRule::match(const Command& cmd) const {
    return cmd.script == "ls" && !cmd.output.empty();
}
std::vector<std::string> LsLahRule::get_new_command(const Command& cmd) const {
    return {"ls -lah"};
}

RULE_CLASS(MkdirPRule);
bool MkdirPRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "mkdir ") &&
           utils::contains(cmd.output, "No such file or directory");
}
std::vector<std::string> MkdirPRule::get_new_command(const Command& cmd) const {
    return {"mkdir -p " + cmd.script.substr(6)};
}

RULE_CLASS(RmDirRule);
bool RmDirRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "rm ") &&
           (utils::contains(cmd.output, "is a directory") ||
            utils::contains(cmd.output, "Is a directory"));
}
std::vector<std::string> RmDirRule::get_new_command(const Command& cmd) const {
    return {"rm -rf " + cmd.script.substr(3)};
}

RULE_CLASS(SlLsRule);
bool SlLsRule::match(const Command& cmd) const {
    return cmd.script == "sl" || utils::starts_with(cmd.script, "sl ");
}
std::vector<std::string> SlLsRule::get_new_command(const Command& cmd) const {
    return {"ls" + cmd.script.substr(2)};
}

RULE_CLASS(PythonCommandRule);
bool PythonCommandRule::match(const Command& cmd) const {
    return utils::contains(cmd.output, "Permission denied") &&
           !cmd.script_parts.empty() &&
           utils::ends_with(cmd.script_parts[0], ".py");
}
std::vector<std::string> PythonCommandRule::get_new_command(const Command& cmd) const {
    return {"python " + cmd.script};
}

RULE_CLASS(PythonExecuteRule);
bool PythonExecuteRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "python ") &&
           utils::contains(cmd.output, "No such file") &&
           !utils::ends_with(cmd.script, ".py");
}
std::vector<std::string> PythonExecuteRule::get_new_command(const Command& cmd) const {
    return {cmd.script + ".py"};
}

RULE_CLASS(JavaRule);
bool JavaRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "java ") &&
           utils::ends_with(cmd.script_parts.back(), ".java");
}
std::vector<std::string> JavaRule::get_new_command(const Command& cmd) const {
    std::string fixed = cmd.script;
    fixed = fixed.substr(0, fixed.length() - 5);
    return {fixed};
}

RULE_CLASS(JavacRule);
bool JavacRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "javac ") &&
           utils::contains(cmd.output, "No such file") &&
           !utils::ends_with(cmd.script, ".java");
}
std::vector<std::string> JavacRule::get_new_command(const Command& cmd) const {
    return {cmd.script + ".java"};
}

RULE_CLASS(GoRunRule);
bool GoRunRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "go run ") &&
           !utils::ends_with(cmd.script, ".go");
}
std::vector<std::string> GoRunRule::get_new_command(const Command& cmd) const {
    return {cmd.script + ".go"};
}

RULE_CLASS(CargoRule);
bool CargoRule::match(const Command& cmd) const {
    return cmd.script == "cargo";
}
std::vector<std::string> CargoRule::get_new_command(const Command& cmd) const {
    return {"cargo build"};
}

RULE_CLASS(DockerNotCommandRule);
bool DockerNotCommandRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "docker ") &&
           utils::contains(cmd.output, "is not a docker command");
}
std::vector<std::string> DockerNotCommandRule::get_new_command(const Command& cmd) const {
    std::map<std::string, std::string> common = {
        {"tags", "images"}, {"tag", "image"}
    };
    if (cmd.script_parts.size() > 1 && common.count(cmd.script_parts[1])) {
        return {"docker " + common[cmd.script_parts[1]]};
    }
    return {cmd.script};
}

RULE_CLASS(NpmWrongCommandRule);
bool NpmWrongCommandRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "npm ") &&
           utils::contains(cmd.output, "Unknown command");
}
std::vector<std::string> NpmWrongCommandRule::get_new_command(const Command& cmd) const {
    std::map<std::string, std::string> typos = {
        {"urgrade", "upgrade"}, {"isntall", "install"},
        {"instal", "install"}, {"intsall", "install"}
    };
    if (cmd.script_parts.size() > 1 && typos.count(cmd.script_parts[1])) {
        return {"npm " + typos[cmd.script_parts[1]]};
    }
    return {cmd.script};
}

RULE_CLASS(PipUnknownCommandRule);
bool PipUnknownCommandRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "pip ") &&
           utils::contains(cmd.output, "unknown command");
}
std::vector<std::string> PipUnknownCommandRule::get_new_command(const Command& cmd) const {
    std::map<std::string, std::string> typos = {
        {"instatl", "install"}, {"instal", "install"},
        {"isntall", "install"}, {"unisntall", "uninstall"}
    };
    if (cmd.script_parts.size() > 1 && typos.count(cmd.script_parts[1])) {
        return {"pip " + typos[cmd.script_parts[1]]};
    }
    return {cmd.script};
}

RULE_CLASS(GitCloneGitCloneRule);
bool GitCloneGitCloneRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "git clone git clone");
}
std::vector<std::string> GitCloneGitCloneRule::get_new_command(const Command& cmd) const {
    return {cmd.script.substr(10)};
}

RULE_CLASS(WrongHyphenBeforeSubcommandRule);
bool WrongHyphenBeforeSubcommandRule::match(const Command& cmd) const {
    return utils::contains(cmd.output, "command not found") &&
           cmd.script_parts.size() > 0 &&
           utils::contains(cmd.script_parts[0], "-");
}
std::vector<std::string> WrongHyphenBeforeSubcommandRule::get_new_command(const Command& cmd) const {
    std::string fixed = cmd.script;
    size_t pos = fixed.find('-');
    if (pos != std::string::npos) {
        fixed[pos] = ' ';
    }
    return {fixed};
}

RULE_CLASS(MissingSpaceBeforeSubcommandRule);
bool MissingSpaceBeforeSubcommandRule::match(const Command& cmd) const {
    return utils::contains(cmd.output, "command not found") &&
           (utils::starts_with(cmd.script, "npm") ||
            utils::starts_with(cmd.script, "git") ||
            utils::starts_with(cmd.script, "apt"));
}
std::vector<std::string> MissingSpaceBeforeSubcommandRule::get_new_command(const Command& cmd) const {
    if (utils::starts_with(cmd.script, "npminstall")) {
        return {"npm install" + cmd.script.substr(10)};
    } else if (utils::starts_with(cmd.script, "gitcommit")) {
        return {"git commit" + cmd.script.substr(9)};
    } else if (utils::starts_with(cmd.script, "aptinstall")) {
        return {"apt install" + cmd.script.substr(10)};
    }
    return {cmd.script};
}

RULE_CLASS(RemoveShellPromptLiteralRule);
bool RemoveShellPromptLiteralRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "$ ");
}
std::vector<std::string> RemoveShellPromptLiteralRule::get_new_command(const Command& cmd) const {
    return {cmd.script.substr(2)};
}

RULE_CLASS(TouchRule);
bool TouchRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "touch ") &&
           utils::contains(cmd.output, "No such file or directory");
}
std::vector<std::string> TouchRule::get_new_command(const Command& cmd) const {
    std::string path = cmd.script.substr(6);
    size_t last_slash = path.rfind('/');
    if (last_slash != std::string::npos) {
        std::string dir = path.substr(0, last_slash);
        return {"mkdir -p " + dir + " && touch " + path};
    }
    return {cmd.script};
}

RULE_CLASS(UnsudoRule);
bool UnsudoRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "sudo ") &&
           (utils::contains(cmd.output, "must not be run as root") ||
            utils::contains(cmd.output, "don't run this as root"));
}
std::vector<std::string> UnsudoRule::get_new_command(const Command& cmd) const {
    return {cmd.script.substr(5)};
}

RULE_CLASS(LnSOrderRule);
bool LnSOrderRule::match(const Command& cmd) const {
    return utils::starts_with(cmd.script, "ln -s") &&
           utils::contains(cmd.output, "No such file or directory");
}
std::vector<std::string> LnSOrderRule::get_new_command(const Command& cmd) const {
    if (cmd.script_parts.size() >= 4) {
        return {"ln -s " + cmd.script_parts[3] + " " + cmd.script_parts[2]};
    }
    return {cmd.script};
}

RULE_CLASS(Cpp11Rule);
bool Cpp11Rule::match(const Command& cmd) const {
    return (utils::starts_with(cmd.script, "g++ ") || utils::starts_with(cmd.script, "clang++ ")) &&
           !utils::contains(cmd.script, "-std=") &&
           (utils::contains(cmd.output, "C++11") || utils::contains(cmd.output, "c++11"));
}
std::vector<std::string> Cpp11Rule::get_new_command(const Command& cmd) const {
    return {cmd.script + " -std=c++11"};
}

RULE_CLASS(GitMainMasterRule);
bool GitMainMasterRule::match(const Command& cmd) const {
    return (utils::contains(cmd.script, "master") && utils::contains(cmd.output, "did you mean 'main'")) ||
           (utils::contains(cmd.script, "main") && utils::contains(cmd.output, "did you mean 'master'"));
}
std::vector<std::string> GitMainMasterRule::get_new_command(const Command& cmd) const {
    if (utils::contains(cmd.script, "master")) {
        std::string fixed = cmd.script;
        size_t pos = fixed.find("master");
        fixed.replace(pos, 6, "main");
        return {fixed};
    } else {
        std::string fixed = cmd.script;
        size_t pos = fixed.find("main");
        fixed.replace(pos, 4, "master");
        return {fixed};
    }
}

// Settings manager
class Settings {
public:
    bool require_confirmation = true;
    bool no_colors = false;
    bool debug = false;
    bool alter_history = true;
    int wait_command = 3;
    int history_limit = 9999;
    int num_close_matches = 3;

    static Settings& instance() {
        static Settings s;
        return s;
    }

private:
    Settings() { load_from_env(); }

    void load_from_env() {
        const char* env_confirm = std::getenv("THESHIT_REQUIRE_CONFIRMATION");
        if (env_confirm) require_confirmation = std::string(env_confirm) == "true";

        const char* env_colors = std::getenv("THESHIT_NO_COLORS");
        if (env_colors) no_colors = std::string(env_colors) == "true";

        const char* env_debug = std::getenv("THESHIT_DEBUG");
        if (env_debug) debug = std::string(env_debug) == "true";
    }
};


// Levenshtein distance calculation
namespace fuzzy {
    int levenshtein_distance(const std::string& s1, const std::string& s2) {
        size_t len1 = s1.length();
        size_t len2 = s2.length();

        std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));

        for (size_t i = 0; i <= len1; i++) dp[i][0] = i;
        for (size_t j = 0; j <= len2; j++) dp[0][j] = j;

        for (size_t i = 1; i <= len1; i++) {
            for (size_t j = 1; j <= len2; j++) {
                if (s1[i - 1] == s2[j - 1]) {
                    dp[i][j] = dp[i - 1][j - 1];
                } else {
                    dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
                }
            }
        }

        return dp[len1][len2];
    }

    // Get all available commands from system paths
    std::vector<std::string> get_system_commands() {
        std::vector<std::string> commands;
        std::set<std::string> seen; // Track unique commands

        const char* path_env = std::getenv("PATH");
        if (!path_env) return commands;

        std::string path_str(path_env);
        std::istringstream iss(path_str);
        std::string path;

        // Split PATH by colons
        while (std::getline(iss, path, ':')) {
            if (!utils::is_directory(path)) continue;

            DIR* dir = opendir(path.c_str());
            if (!dir) continue;

            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string filename = entry->d_name;

                // Skip hidden files and . and ..
                if (filename[0] == '.') continue;

                // Check if it's a regular file (not directory)
                if (entry->d_type != DT_REG && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN) {
                    continue;
                }

                // Check if executable
                std::string full_path = path + "/" + filename;
                struct stat st;
                if (stat(full_path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                    if (seen.find(filename) == seen.end()) {
                        commands.push_back(filename);
                        seen.insert(filename);
                    }
                }
            }
            closedir(dir);
        }

        return commands;
    }

    struct CommandMatch {
        std::string command;
        int distance;
    };

    auto compare_matches = [](const CommandMatch& a, const CommandMatch& b) {
        return a.distance < b.distance;
    };

    // Static cache for commands (avoid repeated filesystem scans)
    class CommandCache {
    private:
        std::vector<std::string> cached_commands;
        bool initialized = false;

    public:
        const std::vector<std::string>& get_commands() {
            if (!initialized) {
                cached_commands = get_system_commands();
                initialized = true;

                if (Settings::instance().debug) {
                    std::cerr << "Loaded " << cached_commands.size() << " system commands\n";
                }
            }
            return cached_commands;
        }
    };

    CommandCache& get_command_cache() {
        static CommandCache cache;
        return cache;
    }

    std::vector<CommandMatch> find_similar_commands(const std::string& input, int max_distance = 2) {
        std::vector<CommandMatch> matches;
        const auto& commands = get_command_cache().get_commands();

        for (const auto& cmd : commands) {
            int dist = levenshtein_distance(input, cmd);
            if (dist <= max_distance) {
                matches.push_back({cmd, dist});
            }
        }

        std::sort(matches.begin(), matches.end(), compare_matches);
        return matches;
    }
}

// Fuzzy Rule - tries to match typos in the main command
RULE_CLASS(FuzzyCommandRule);
bool FuzzyCommandRule::match(const Command& cmd) const {
    // Only match if "command not found" error
    if (!utils::contains(cmd.output, "command not found")) {
        return false;
    }

    if (cmd.script_parts.empty()) {
        return false;
    }

    // Check if there are similar commands
    auto matches = fuzzy::find_similar_commands(cmd.script_parts[0]);
    return !matches.empty();
}

std::vector<std::string> FuzzyCommandRule::get_new_command(const Command& cmd) const {
    if (cmd.script_parts.empty()) {
        return {};
    }

    auto matches = fuzzy::find_similar_commands(cmd.script_parts[0], 3);
    std::vector<std::string> suggestions;

    // Create suggestions for each close match
    for (size_t i = 0; i < matches.size() && i < 3; i++) {
        std::string fixed = matches[i].command;
        for (size_t j = 1; j < cmd.script_parts.size(); j++) {
            fixed += " " + cmd.script_parts[j];
        }
        suggestions.push_back(fixed);
    }

    return suggestions;
}

// Rule Manager
class RuleManager {
private:
    std::vector<std::unique_ptr<Rule>> rules;

public:
    RuleManager() {
        // Register all rules
        rules.push_back(std::make_unique<SudoRule>());
        rules.push_back(std::make_unique<FuzzyCommandRule>());
        rules.push_back(std::make_unique<GitPushRule>());
        rules.push_back(std::make_unique<NoCommandRule>());
        rules.push_back(std::make_unique<GitNotCommandRule>());
        rules.push_back(std::make_unique<CdMkdirRule>());
        rules.push_back(std::make_unique<CdParentRule>());
        rules.push_back(std::make_unique<CdCsRule>());
        rules.push_back(std::make_unique<CatDirRule>());
        rules.push_back(std::make_unique<ChmodXRule>());
        rules.push_back(std::make_unique<CpOmittingDirectoryRule>());
        rules.push_back(std::make_unique<DryRule>());
        rules.push_back(std::make_unique<GitAddRule>());
        rules.push_back(std::make_unique<GitAddForceRule>());
        rules.push_back(std::make_unique<GitBranchDeleteRule>());
        rules.push_back(std::make_unique<GitCommitAddRule>());
        rules.push_back(std::make_unique<GitCommitAmendRule>());
        rules.push_back(std::make_unique<GitPullRule>());
        rules.push_back(std::make_unique<GitTwoDashesRule>());
        rules.push_back(std::make_unique<GrepRecursiveRule>());
        rules.push_back(std::make_unique<HasExistsScriptRule>());
        rules.push_back(std::make_unique<LsAllRule>());
        rules.push_back(std::make_unique<LsLahRule>());
        rules.push_back(std::make_unique<MkdirPRule>());
        rules.push_back(std::make_unique<RmDirRule>());
        rules.push_back(std::make_unique<SlLsRule>());
        rules.push_back(std::make_unique<PythonCommandRule>());
        rules.push_back(std::make_unique<PythonExecuteRule>());
        rules.push_back(std::make_unique<JavaRule>());
        rules.push_back(std::make_unique<JavacRule>());
        rules.push_back(std::make_unique<GoRunRule>());
        rules.push_back(std::make_unique<CargoRule>());
        rules.push_back(std::make_unique<DockerNotCommandRule>());
        rules.push_back(std::make_unique<NpmWrongCommandRule>());
        rules.push_back(std::make_unique<PipUnknownCommandRule>());
        rules.push_back(std::make_unique<GitCloneGitCloneRule>());
        rules.push_back(std::make_unique<WrongHyphenBeforeSubcommandRule>());
        rules.push_back(std::make_unique<MissingSpaceBeforeSubcommandRule>());
        rules.push_back(std::make_unique<RemoveShellPromptLiteralRule>());
        rules.push_back(std::make_unique<TouchRule>());
        rules.push_back(std::make_unique<UnsudoRule>());
        rules.push_back(std::make_unique<LnSOrderRule>());
        rules.push_back(std::make_unique<Cpp11Rule>());
        rules.push_back(std::make_unique<GitMainMasterRule>());

        // Sort by priority
        std::sort(rules.begin(), rules.end(),
                  [](const auto& a, const auto& b) {
                      return a->get_priority() < b->get_priority();
                  });
    }

    std::vector<std::string> get_corrected_commands(const Command& cmd) {
        for (const auto& rule : rules) {
            if (rule->match(cmd)) {
                if (Settings::instance().debug) {
                    // std::cerr << "Matched rule: " << rule->get_name() << std::endl;
                }
                return rule->get_new_command(cmd);
            }
        }
        return {};
    }
};

std::string get_last_command() {
    const char* shell = std::getenv("SHELL");
    const char* home = std::getenv("HOME");

    if (!home) return "";

    std::string histfile;
    bool is_zsh = shell && std::string(shell).find("zsh") != std::string::npos;

    if (is_zsh) {
        histfile = std::string(home) + "/.zsh_history";
    } else {
        histfile = std::string(home) + "/.bash_history";
    }

    std::ifstream file(histfile);
    std::string last_line;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::string cmd = line;

        // For zsh: format is `: timestamp:0;command`
        if (is_zsh) {
            size_t semicolon_pos = line.rfind(';');
            if (semicolon_pos != std::string::npos) {
                cmd = line.substr(semicolon_pos + 1);
            }
        }

        // Trim leading/trailing whitespace
        size_t start = cmd.find_first_not_of(" \t\n\r");
        size_t end = cmd.find_last_not_of(" \t\n\r");

        if (start != std::string::npos) {
            cmd = cmd.substr(start, end - start + 1);
        } else {
            continue;
        }

        // Skip empty commands and shit commands
        if (!cmd.empty() && cmd.find("shit") == std::string::npos &&
            cmd.find("nano") == std::string::npos) {
            last_line = cmd;
            }
    }

    return last_line;
}

// Execute command and capture output
std::string execute_command(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return "";

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

int main(int argc, char* argv[]) {
    bool yes_mode = false;
    bool recursive = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--yeah" || arg == "-y" || arg == "--hard") {
            yes_mode = true;
        } else if (arg == "-r") {
            recursive = true;
        } else if (arg == "--alias") {
            std::cout << "alias shit='eval $(theshit $(fc -ln -1))'\n";
            return 0;
        } else if (arg == "--version") {
            std::cout << "The Shit v1.0.0 (C++ Edition)\n";
            return 0;
        }
    }

    // Get last command
    std::string last_cmd = get_last_command();
    if (last_cmd.empty()) {
        // std::cerr << "No previous command found\n";
        return 1;
    }

    // DEBUG: Print what we extracted
    // std::cerr << "DEBUG: Extracted command: [" << last_cmd << "]\n";

    // Execute the command to get its output
    std::string output = execute_command(last_cmd);

    // DEBUG: Print the output
    // std::cerr << "DEBUG: Command output: [" << output << "]\n";

    Command cmd(last_cmd, output);

    int attempts = 0;
    const int max_attempts = recursive ? 10 : 1;

    while (attempts < max_attempts) {
        RuleManager manager;
        auto corrections = manager.get_corrected_commands(cmd);

        if (corrections.empty()) {
            if (attempts == 0) {
                std::cout << "No shit to fix!\n";
            }
            break;
        }

        const std::string& correction = corrections[0];

        if (!Settings::instance().no_colors) {
            std::cout << "\033[1;32m" << correction << "\033[0m";
        } else {
            std::cout << correction;
        }

        if (!yes_mode && Settings::instance().require_confirmation) {
            std::cout << " [enter/↑/↓/ctrl+c]\n";
            std::cin.get();
        } else {
            std::cout << std::endl;
        }

        // Execute the corrected command
        int result = system(correction.c_str());

        if (result == 0 || !recursive) {
            break;
        }

        // Prepare for next iteration in recursive mode
        output = execute_command(correction);
        cmd = Command(correction, output);
        attempts++;
    }

    return 0;
}