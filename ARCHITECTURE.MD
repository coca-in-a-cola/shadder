# Super Shadder — Architecture

## Overview
Реалтайм-графика на D3D11, C++17, SCons, Windows-only (clang-cl).

## Directory Layout
```
/                   — корень проекта
├── src/             — исходный код (фреймворк, без точки входа)
│   ├── core/        — утилиты (Callable/Signal, KeyCodes), ECS, prefabs/scenes
│   │   └── ecs/     — ECS (World, Query, Storage, Component, System)
│   ├── dev/         — слой устройств (display/win32, input/raw)
│   ├── framework/   — игровой фреймворк
│   │   ├── game/    — Game (D3D11 init, loop, ImGui)
│   │   └── modules/ — ECS-модули
│   │       ├── transform/   — Transform3D
│   │       ├── camera/      — CameraComponent, CameraSystem
│   │       ├── render/      — Mesh, Material, RenderSystem, Text, ObjLoader
│   │       └── physics/     — Velocity, Collider, Movement, Collision
│   ├── shared/      — header-only префабы для примеров (Camera/Mesh)
│   └── util/        — Callable, KeyCodes
├── include/         — сгенерированные заголовки (shadder.hpp — namespace wrapper)
├── data/            — общие ассеты (shaders/, models/)
├── examples/        — самостоятельные примеры-проекты, каждый со своим SConstruct
│                    (framework/, pong/, solar/, katamari/, phong/)
├── tools/launcher/  — ImGui-лаунчер: меню сборки/запуска примеров (визуальное демо)
├── scons/           — SCons-утилиты (scons_hints, generate_wrapper)
├── thirdparty/      — зависимости (авто-загрузка DirectXTK, imgui/)
├── bin/             — выход сборки (Launcher.exe + data/)
└── SConstruct       — корневой файл сборки
```

## Build System (SCons)
- **VariantDir**: `bin/ → src/` (без дублирования)
- **Auto-discovery**: все `.cpp` из дерева `src/` собираются автоматически
  (`collect_module_sources`), SCsub-файлов нет
- **Цель**: `bin/Launcher.exe` — framework + imgui + `tools/launcher/src/`
  (объекты в `bin/launcher/`)
- **Examples**: собираются независимо — `scons -C examples/<name> debug=0|1`,
  каждый SConstruct подключает `src/` репозитория (минус `main/`, `shared/`)
  + imgui и копирует общие ассеты `data/` в свой `bin/`
- **Options**: `debug` (0/1), `verbose`, `compiledb`, `compiledb_gen_only`
- **Compilation Database**: `compile_commands.json` для clangd
- **Toolchain**: `clang-cl` (clang-фронтенд с MSVC-линкером), C++17, `/EHsc`
- **Thirdparty**: DirectXTK автоматически скачивается из NuGet при первом запуске

## Launcher (tools/launcher)
- Визуальное демо фреймворка, **не** точка входа и не example
- Скан `examples/*/SConstruct` → парс `PROGRAM_NAME` → `bin/<Name>.exe`
- Кнопки Run / Build / Build+Run (scons с CWD примера, лог через pipe),
  Stop, переключение примеров; один child-процесс одновременно
- Job Object (kill-on-close): отмена сборки убивает дерево scons,
  закрытие лаунчера — запущенный пример

## ECS Architecture
- **Entity**: `{index: uint32_t, generation: uint32_t}` — безопасный handle с поколениями
- **Storage**: `SparseSetStorage<T>` — dense array компонентов + sparse map для O(1) доступа
- **World**: владеет стораджами и системами, управляет жизненным циклом сущностей
- **System phases**: `UPDATE → PRE_RENDER → RENDER → POST_RENDER`
- **Query**: multi-component итерация — выбирает наименьший dense set, проверяет пересечение
- **Module pattern**: `register_types.h/cpp` с `initialize_*_module(World&)` — явная инициализация в Game

## Key Patterns
- **Callable/Signal**: Godot-стайл делегаты и события (InputDevice, CollisionSystem)
- **Game loop**: `Game::Run()` → MessagePump → PrepareFrame → Update(dt) → Draw → EndFrame
- **D3D11 pipeline**: Device → SwapChain → BackBuffer → RTV → Clear → Draw → Present
- **Namespace wrapper**: авто-генерируемый `include/shadder.hpp` приносит все символы в `namespace shadder`

## Coding Conventions
- `#pragma once`, C++17, DirectXMath
- 4-space indent, tabs for C++ (clang-format: LLVM-based)
- `ComPtr` для D3D, виртуальные хуки в Game
- Комментарии на русском
