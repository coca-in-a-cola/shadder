# Super Shadder — Architecture

## Overview
Реалтайм-графика на D3D11, C++17, SCons, Windows-only (clang-cl).

## Directory Layout
```
/                   — корень проекта
├── src/             — исходный код
│   ├── main/        — точка входа (main.cpp)
│   ├── core/        — утилиты (Callable/Signal, KeyCodes)
│   ├── ecs/         — ECS (header-only: World, Query, Storage, Component, System)
│   ├── dev/         — слой устройств (display/win32, input/raw)
│   └── framework/   — игровой фреймворк
│       ├── game/    — Game (D3D11 init, loop)
│       └── modules/ — ECS-модули
│           ├── transform/   — Transform3D
│           ├── render/      — Mesh, Material, RenderSystem
│           └── physics/     — Velocity, Collider, Movement, Collision
├── include/         — сгенерированные заголовки (shadder.hpp — namespace wrapper)
├── data/            — ассеты (shaders/, models/)
├── examples/        — примеры проектов (framework/, pong/)
├── scons/           — SCons-утилиты (scons_hints, generate_wrapper)
├── thirdparty/      — зависимости (авто-загрузка DirectXTK)
├── bin/             — выход сборки
└── SConstruct       — корневой файл сборки
```

## Build System (SCons)
- **VariantDir**: `bin/ → src/` (без дублирования)
- **Module tree**: каждый модуль = директория с `SCsub`, который добавляет `.cpp` через `env.add_source_files(env.module_sources, ...)`
- **SCsub hierarchy**: `src/SCsub` → `main/`, `core/`, `ecs/`, `dev/`, `framework/SCsub` → рекурсивно вложенные SCsub
- **Options**: `verbose`, `compiledb`, `compiledb_gen_only`
- **Compilation Database**: `compile_commands.json` для clangd
- **Toolchain**: `clang-cl` (clang-фронтенд с MSVC-линкером), C++17, `/EHsc`
- **Thirdparty**: DirectXTK автоматически скачивается из NuGet при первом запуске

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
