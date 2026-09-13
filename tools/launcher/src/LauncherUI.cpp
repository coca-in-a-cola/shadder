#include "LauncherUI.h"

#include "framework/game/Game.h"

#include <imgui.h>

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace {

constexpr int kMaxLogLines = 500;

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                   nullptr, 0, nullptr, nullptr);
    std::string out(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::string PathToUtf8(const fs::path& p) {
    return WideToUtf8(p.wstring());
}

std::string LastErrorText() {
    wchar_t* msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&msg, 0, nullptr);
    std::string out = msg ? WideToUtf8(msg) : "unknown error";
    if (msg) LocalFree(msg);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

// Job Object с kill-on-close: закрытие хэндла (или смерть лаунчера) убивает
// всё дерево процессов — scons/python при отмене сборки, exe примера при
// закрытии лаунчера.
HANDLE MakeKillOnCloseJob() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                            sizeof(info));
    return job;
}

bool IsRepoRoot(const fs::path& p) {
    return fs::exists(p / "examples") && fs::exists(p / "src") &&
           fs::exists(p / "scons");
}

std::string ReadFileText(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

// ---------------------------------------------------------------------------

LauncherUI::LauncherUI(Game* g) : game_(g) {
    FindRepoRoot();
    ScanExamples();
    ResolveScons();
    AppendLog("[Launcher] Repo: " +
              (repoRoot_.empty() ? std::string("<not found>") : PathToUtf8(repoRoot_)));
    if (sconsPath_.empty())
        AppendLog("[Launcher] scons не найден — сборка недоступна");
    else
        AppendLog("[Launcher] scons: " + PathToUtf8(sconsPath_));
}

LauncherUI::~LauncherUI() {
    StopRunning();
    CancelBuild();
}

void LauncherUI::FindRepoRoot() {
    repoRoot_.clear();

    // Сначала поднимаемся от exe: <repo>/bin/Launcher.exe → <repo>.
    wchar_t exeBuf[MAX_PATH];
    GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    for (fs::path p = fs::path(exeBuf).parent_path(); !p.empty();
         p = p.parent_path()) {
        if (IsRepoRoot(p)) {
            repoRoot_ = p;
            return;
        }
        if (p == p.parent_path()) break;
    }

    // Fallback: поднимаемся от текущей рабочей директории.
    for (fs::path p = fs::current_path(); !p.empty(); p = p.parent_path()) {
        if (IsRepoRoot(p)) {
            repoRoot_ = p;
            return;
        }
        if (p == p.parent_path()) break;
    }
}

std::string LauncherUI::ParseProgramName(const std::string& text) {
    static const std::regex re(R"(PROGRAM_NAME\s*=\s*['"]([^'"]+)['"])");
    std::smatch m;
    if (std::regex_search(text, m, re)) return m[1].str();
    return {};
}

void LauncherUI::ScanExamples() {
    examples_.clear();
    if (repoRoot_.empty()) return;

    std::error_code ec;
    for (fs::directory_iterator it(fs::path(repoRoot_) / "examples", ec), end;
         it != end && !ec; it.increment(ec)) {
        if (!it->is_directory()) continue;
        std::string dirName = WideToUtf8(it->path().filename().wstring());
        if (dirName == "launcher") continue;

        fs::path sconstruct = it->path() / "SConstruct";
        if (!fs::exists(sconstruct)) continue;

        ExampleInfo ex;
        ex.dirName = dirName;
        ex.buildDir = it->path();
        ex.binDir = it->path() / "bin";
        ex.program = ParseProgramName(ReadFileText(sconstruct));
        if (ex.program.empty()) ex.program = dirName; // fallback
        ex.exePath = ex.binDir / (ex.program + ".exe");
        ex.exeExists = fs::exists(ex.exePath);
        examples_.push_back(std::move(ex));
    }

    std::sort(examples_.begin(), examples_.end(),
              [](const ExampleInfo& a, const ExampleInfo& b) {
                  return a.dirName < b.dirName;
              });
}

void LauncherUI::ResolveScons() {
    sconsPath_.clear();
    sconsIsBatch_ = false;
    if (repoRoot_.empty()) return;

    // 1) venv проекта, если появится (uv/venv).
    fs::path venv = fs::path(repoRoot_) / ".venv" / "Scripts" / "scons.exe";
    if (fs::exists(venv)) {
        sconsPath_ = venv;
        return;
    }

    // 2) Поиск на PATH: exe — напрямую, bat/cmd — через cmd.exe /c call.
    for (const wchar_t* name : {L"scons.exe", L"scons.bat", L"scons.cmd"}) {
        wchar_t buf[MAX_PATH];
        wchar_t* filePart = nullptr;
        DWORD n = SearchPathW(nullptr, name, nullptr, MAX_PATH, buf, &filePart);
        if (n > 0 && n < MAX_PATH) {
            sconsPath_ = buf;
            sconsIsBatch_ = std::wstring(name).find(L".exe") == std::wstring::npos;
            return;
        }
    }

    // 3) Fallback: пусть cmd сам разрулит PATH/PATHEXT.
    sconsPath_ = L"scons";
    sconsIsBatch_ = true;
}

// ---------------------------------------------------------------------------

void LauncherUI::AppendLog(const std::string& line) {
    std::string::size_type pos = 0;
    while (pos <= line.size()) {
        std::string::size_type nl = line.find('\n', pos);
        if (nl == std::string::npos) {
            logLines_.push_back(line.substr(pos));
            break;
        }
        logLines_.push_back(line.substr(pos, nl - pos));
        pos = nl + 1;
    }
    while (logLines_.size() > kMaxLogLines) logLines_.pop_front();
}

void LauncherUI::PumpBuildLog() {
    if (!buildPipeRead_) return;
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(buildPipeRead_, nullptr, 0, nullptr, &avail, nullptr))
            break;
        if (avail == 0) break;
        char buf[4096];
        DWORD toRead = std::min<DWORD>(avail, sizeof(buf));
        DWORD read = 0;
        if (!ReadFile(buildPipeRead_, buf, toRead, &read, nullptr) || read == 0)
            break;
        buildPendingLog_.append(buf, read);
        std::string::size_type pos = 0;
        for (;;) {
            std::string::size_type nl = buildPendingLog_.find('\n', pos);
            if (nl == std::string::npos) break;
            std::string line = buildPendingLog_.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) AppendLog(line);
            pos = nl + 1;
        }
        buildPendingLog_.erase(0, pos);
    }
}

bool LauncherUI::StartBuild(int index, bool launchAfter) {
    if (buildState_ == ChildState::Building) return false;
    if (index < 0 || index >= (int)examples_.size()) return false;
    if (repoRoot_.empty()) return false;
    if (sconsPath_.empty()) {
        AppendLog("[Launcher] scons не найден — сборка невозможна");
        return false;
    }

    ExampleInfo& ex = examples_[index];
    if (runningIndex_ == index) {
        AppendLog("[Launcher] Нельзя собирать запущенный пример: " + ex.dirName +
                  " (exe занят). Сначала Stop.");
        return false;
    }

    // Пайп для stdout/stderr процесса сборки.
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE readH = nullptr, writeH = nullptr;
    if (!CreatePipe(&readH, &writeH, &sa, 0)) {
        AppendLog("[Launcher] CreatePipe failed: " + LastErrorText());
        return false;
    }
    SetHandleInformation(writeH, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    std::wstring args = L" debug=" + std::to_wstring(debugLevel_);
    std::wstring cmd;
    if (sconsIsBatch_)
        cmd = L"cmd.exe /c call \"" + sconsPath_.wstring() + L"\"" + args;
    else
        cmd = L"\"" + sconsPath_.wstring() + L"\"" + args;

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nullptr;
    si.hStdOutput = writeH;
    si.hStdError = writeH;

    PROCESS_INFORMATION pi = {};
    std::wstring workDir = ex.buildDir.wstring();
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                             CREATE_SUSPENDED,
                             nullptr, workDir.c_str(), &si, &pi);
    CloseHandle(writeH);
    if (!ok) {
        CloseHandle(readH);
        AppendLog("[Launcher] Failed to start scons for " + ex.dirName + ": " +
                  LastErrorText());
        return false;
    }
    buildProcess_ = pi.hProcess;
    buildJob_ = MakeKillOnCloseJob();
    if (buildJob_ && !AssignProcessToJobObject(buildJob_, buildProcess_)) {
        TerminateProcess(buildProcess_, (UINT)-1);
        CloseHandle(pi.hThread);
        CloseHandle(buildProcess_);
        CloseHandle(buildJob_);
        CloseHandle(readH);
        buildProcess_ = nullptr;
        buildJob_ = nullptr;
        AppendLog("[Launcher] Failed to assign build process to Job Object: " + LastErrorText());
        return false;
    }
    if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) {
        TerminateProcess(buildProcess_, (UINT)-1);
        CloseHandle(pi.hThread);
        CloseHandle(buildProcess_);
        if (buildJob_) CloseHandle(buildJob_);
        CloseHandle(readH);
        buildProcess_ = nullptr;
        buildJob_ = nullptr;
        AppendLog("[Launcher] Failed to resume build process: " + LastErrorText());
        return false;
    }
    CloseHandle(pi.hThread);
    buildPipeRead_ = readH;
    buildPendingLog_.clear();
    buildingIndex_ = index;
    buildState_ = ChildState::Building;
    launchAfterBuild_ = launchAfter;
    ex.hasBuildResult = false;

    AppendLog("[Launcher] Building " + ex.dirName + " (debug=" +
              std::to_string(debugLevel_) + ")...");
    return true;
}

void LauncherUI::BuildAll() {
    if (buildState_ == ChildState::Building) return;
    buildQueue_.clear();
    for (int i = 0; i < (int)examples_.size(); ++i) buildQueue_.push_back(i);
    PumpBuildQueue();
}

bool LauncherUI::PumpBuildQueue() {
    while (!buildQueue_.empty()) {
        int next = buildQueue_.front();
        buildQueue_.erase(buildQueue_.begin());
        if (StartBuild(next, false)) return true;
    }
    return false;
}

void LauncherUI::CancelBuild() {
    buildQueue_.clear();
    launchAfterBuild_ = false;
    if (buildState_ != ChildState::Building) return;
    if (buildJob_) TerminateJobObject(buildJob_, (UINT)-1);
    WaitForSingleObject(buildProcess_, 3000);
    if (buildJob_) CloseHandle(buildJob_);
    CloseHandle(buildProcess_);
    CloseHandle(buildPipeRead_);
    buildJob_ = nullptr;
    buildProcess_ = nullptr;
    buildPipeRead_ = nullptr;
    buildState_ = ChildState::None;
    buildingIndex_ = -1;
    AppendLog("[Launcher] Build cancelled");
}

void LauncherUI::PollBuild() {
    if (buildState_ != ChildState::Building) return;
    PumpBuildLog();

    if (WaitForSingleObject(buildProcess_, 0) != WAIT_OBJECT_0) return;

    PumpBuildLog(); // дочитываем хвост
    if (!buildPendingLog_.empty()) {
        AppendLog(buildPendingLog_);
        buildPendingLog_.clear();
    }
    DWORD code = 0;
    GetExitCodeProcess(buildProcess_, &code);
    if (buildJob_) CloseHandle(buildJob_);
    CloseHandle(buildProcess_);
    CloseHandle(buildPipeRead_);
    buildJob_ = nullptr;
    buildProcess_ = nullptr;
    buildPipeRead_ = nullptr;

    int doneIndex = buildingIndex_;
    ExampleInfo& ex = examples_[doneIndex];
    bool ok = (code == 0);
    ex.hasBuildResult = true;
    ex.lastBuildOk = ok;
    ex.exeExists = fs::exists(ex.exePath);
    buildState_ = ChildState::None;
    buildingIndex_ = -1;
    AppendLog(ok ? "[Launcher] Build OK: " + ex.dirName
                 : "[Launcher] Build FAILED: " + ex.dirName + " (code " +
                       std::to_string(code) + ")");

    bool launch = launchAfterBuild_ && ok;
    launchAfterBuild_ = false;
    if (launch)
        StartExample(doneIndex);
    else
        PumpBuildQueue(); // очередь Build All
}

void LauncherUI::StartExample(int index) {
    if (index < 0 || index >= (int)examples_.size()) return;
    ExampleInfo& ex = examples_[index];
    if (runningIndex_ == index) return;
    if (!ex.exeExists) {
        AppendLog("[Launcher] Нет exe: " + ex.dirName + " — сначала Build");
        return;
    }
    if (runningIndex_ != -1) StopRunning(); // переключение

    std::wstring cmd = L"\"" + ex.exePath.wstring() + L"\"";
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::wstring workDir = ex.binDir.wstring();
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, workDir.c_str(), &si, &pi)) {
        AppendLog("[Launcher] Failed to start " + ex.dirName + ": " +
                  LastErrorText());
        return;
    }
    CloseHandle(pi.hThread);
    childProcess_ = pi.hProcess;
    childJob_ = MakeKillOnCloseJob();
    if (childJob_) AssignProcessToJobObject(childJob_, childProcess_);
    childPid_ = pi.dwProcessId;
    runningIndex_ = index;
    AppendLog("[Launcher] Started " + ex.dirName + " (pid " +
              std::to_string(childPid_) + ")");
}

void LauncherUI::StopRunning() {
    if (runningIndex_ == -1) return;
    std::string name = examples_[runningIndex_].dirName;
    AppendLog("[Launcher] Stopping " + name + "...");
    TerminateProcess(childProcess_, (UINT)-1);
    WaitForSingleObject(childProcess_, 3000);
    if (childJob_) CloseHandle(childJob_);
    CloseHandle(childProcess_);
    childJob_ = nullptr;
    childProcess_ = nullptr;
    runningIndex_ = -1;
}

void LauncherUI::PollChild() {
    if (runningIndex_ == -1) return;
    if (WaitForSingleObject(childProcess_, 0) != WAIT_OBJECT_0) return;
    DWORD code = 0;
    GetExitCodeProcess(childProcess_, &code);
    if (childJob_) CloseHandle(childJob_);
    CloseHandle(childProcess_);
    childJob_ = nullptr;
    childProcess_ = nullptr;
    AppendLog("[Launcher] " + examples_[runningIndex_].dirName +
              " exited (code " + std::to_string(code) + ")");
    runningIndex_ = -1;
}

// ---------------------------------------------------------------------------

void LauncherUI::OnUpdate(World&, float) {
    PollChild();
    PollBuild();
    DrawUI();
}

void LauncherUI::DrawUI() {
    ImGui::SetNextWindowSize(ImVec2(820, 540), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("LABY Demo Launcher")) {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Repo: %s",
                        repoRoot_.empty() ? "<not found>" : PathToUtf8(repoRoot_).c_str());
    ImGui::TextDisabled("scons: %s",
                        sconsPath_.empty() ? "<not found>" : PathToUtf8(sconsPath_).c_str());

    DrawToolbar();
    ImGui::SeparatorText("Examples");

    if (repoRoot_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                           "Repo root not found — запускай Launcher.exe из bin/ проекта");
    } else if (examples_.empty()) {
        ImGui::TextDisabled("Нет примеров в examples/");
    } else {
        if (ImGui::BeginTable("examples", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Exe", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 240.0f);
            ImGui::TableHeadersRow();
            for (int i = 0; i < (int)examples_.size(); ++i) DrawExampleRow(i);
            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText("Log");
    if (ImGui::BeginChild("logchild", ImVec2(0, ImGui::GetContentRegionAvail().y),
                          true)) {
        bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;
        for (const std::string& line : logLines_)
            ImGui::TextUnformatted(line.c_str());
        if (atBottom) ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}

void LauncherUI::DrawToolbar() {
    ImGui::RadioButton("Debug", &debugLevel_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Release", &debugLevel_, 0);
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    ImGui::BeginDisabled(buildState_ == ChildState::Building || repoRoot_.empty());
    if (ImGui::Button("Build All")) BuildAll();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(buildState_ == ChildState::Building || runningIndex_ != -1);
    if (ImGui::Button("Refresh")) {
        FindRepoRoot();
        ScanExamples();
        ResolveScons();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(runningIndex_ == -1);
    if (ImGui::Button("Stop")) StopRunning();
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Exit")) {
        CancelBuild();
        StopRunning();
        game_->Exit();
    }

    if (buildState_ == ChildState::Building) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel build")) CancelBuild();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1), "building %s...",
                           examples_[buildingIndex_].dirName.c_str());
    }
}

void LauncherUI::DrawExampleRow(int index) {
    ExampleInfo& ex = examples_[index];
    bool isBuilding = (buildingIndex_ == index && buildState_ == ChildState::Building);
    bool isRunning = (runningIndex_ == index);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(ex.dirName.c_str());
    if (ImGui::IsItemHovered() && ex.program != ex.dirName)
        ImGui::SetTooltip("exe: %s", ex.program.c_str());

    ImGui::TableSetColumnIndex(1);
    if (isRunning)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1), "running");
    else if (isBuilding)
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1), "building");
    else if (!ex.exeExists)
        ImGui::TextDisabled("no exe");
    else if (ex.hasBuildResult && !ex.lastBuildOk)
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1), "build fail");
    else
        ImGui::TextUnformatted("ready");

    ImGui::TableSetColumnIndex(2);
    ImGui::TextDisabled("%s", PathToUtf8(ex.exePath).c_str());

    ImGui::TableSetColumnIndex(3);
    ImGui::PushID(index);

    ImGui::BeginDisabled(!ex.exeExists || isBuilding);
    if (ImGui::Button("Run")) StartExample(index);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(buildState_ == ChildState::Building || isRunning);
    if (ImGui::Button("Build")) StartBuild(index, false);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(buildState_ == ChildState::Building || isRunning);
    if (ImGui::Button("Build+Run")) StartBuild(index, true);
    ImGui::EndDisabled();

    ImGui::PopID();
}
