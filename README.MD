# Super Shadder

<img src="logo.png" alt="logo" width="360" height="360"/>

Проект для курса по компьютерной графике.

## Требования

- Windows 10/11.
- Python 3.11+ и SCons 4.10+.
- LLVM/Clang с `clang-cl` в `PATH`.
- Visual Studio Build Tools, C++ toolchain и Windows SDK.
- Direct3D 11 и совместимый GPU.
- Интернет при первой сборке: DirectXTK скачивается автоматически.

## Запуск

Сначала соберите и запустите Launcher из корня проекта:

```powershell
scons debug=1
.\bin\Launcher.exe
```

Launcher предназначен для удобного запуска примеров. В нём можно выбрать `Build`, `Build+Run` или `Run`.

Запуск Launcher одной командой:

```powershell
scons run debug=1
```

## Запуск примера напрямую

```powershell
cd examples\pong
scons debug=1
.\bin\Pong.exe
```

Доступные примеры: `framework`, `katamari`, `phong`, `pong`, `solar`.

Для запуска примера через SCons:

```powershell
cd examples\solar
scons run debug=1
```
