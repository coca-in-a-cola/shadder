#pragma once

#include "core/ecs/System.h"

#include <windows.h>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

class Game;

// Визуальное демо фреймворка: меню-лаунчер, которое сканирует examples/,
// собирает выбранный пример через scons и запускает его exe как дочерний
// процесс. Не является точкой входа фреймворка — каждый пример остаётся
// самостоятельным проектом.
class LauncherUI : public ISystem {
  public:
  explicit LauncherUI(Game* g);
  ~LauncherUI() override;

  void OnUpdate(World& world, float deltaTime) override;

  private:
  struct ExampleInfo {
    std::string dirName;   // имя папки в examples/
    std::string program;   // PROGRAM_NAME из SConstruct
    std::filesystem::path buildDir; // <repo>/examples/<dir>
    std::filesystem::path binDir;   // <repo>/examples/<dir>/bin
    std::filesystem::path exePath;  // bin/<program>.exe
    bool exeExists = false;
    bool lastBuildOk = false;
    bool hasBuildResult = false;
  };

  enum class ChildState {
    None,
    Building,
    Running,
  };

  // --- сканирование и окружение ---
  void FindRepoRoot();
  void ScanExamples();
  void ResolveScons();
  static std::string ParseProgramName(const std::string& sconstructText);

  // --- процессы ---
  bool StartBuild(int index, bool launchAfter);
  void BuildAll();
  bool PumpBuildQueue();
  void CancelBuild();
  void StartExample(int index);
  void StopRunning();
  void PollBuild();
  void PollChild();
  void PumpBuildLog();
  void AppendLog(const std::string& line);

  // --- UI ---
  void DrawUI();
  void DrawToolbar();
  void DrawExampleRow(int index);

  Game* game_;
  std::vector<ExampleInfo> examples_;
  std::filesystem::path repoRoot_;
  std::filesystem::path sconsPath_;  // путь к scons (.exe/.bat) или пусто
  bool sconsIsBatch_ = false;        // .bat/.cmd — запускать через cmd.exe /c call

  int debugLevel_ = 1;               // 1 = Debug, 0 = Release (аргумент scons debug=)

  // Сборка (один слот одновременно)
  ChildState buildState_ = ChildState::None;
  int buildingIndex_ = -1;
  bool launchAfterBuild_ = false;
  std::vector<int> buildQueue_;      // очередь Build All
  HANDLE buildProcess_ = nullptr;
  HANDLE buildJob_ = nullptr;        // Job Object — убивает всё дерево сборки
  HANDLE buildPipeRead_ = nullptr;
  std::string buildPendingLog_;

  // Запущенный пример (один одновременно)
  int runningIndex_ = -1;
  HANDLE childProcess_ = nullptr;
  HANDLE childJob_ = nullptr;        // Job Object — умирает вместе с лаунчером
  DWORD childPid_ = 0;

  std::deque<std::string> logLines_;
  bool autoScroll_ = true;
};
