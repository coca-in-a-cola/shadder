"""Общие хелперы сборки для корневого SConstruct и examples/*/SConstruct.

Выносят код, который раньше был скопирован в каждом SConstruct: окружение
(clang-cl, пути репозитория, D3D-библиотеки), сбор объектов фреймворка/imgui,
копию data-ассетов и run-цель (`scons run`).

Пример SConstruct:

    import os
    import sys

    BUILD_DIR = 'bin'
    PROGRAM_NAME = 'Pong'

    REPO_ROOT = os.path.abspath(os.path.join(Dir('#').abspath, '..', '..'))
    sys.path.insert(0, REPO_ROOT)
    from scons.utility.scons_hints import *     # noqa: E402,F401,F403
    from scons.utility.example_build import *   # noqa: E402,F401,F403

    VariantDir(BUILD_DIR, 'src', duplicate=False)

    env = make_example_env(REPO_ROOT)

    prog = env.Program(
        target=f'{BUILD_DIR}/{PROGRAM_NAME}',
        source=collect_tree_sources(env, BUILD_DIR)
               + collect_framework_objects(env, REPO_ROOT, BUILD_DIR)
               + collect_imgui_objects(env, REPO_ROOT, BUILD_DIR),
    )
    Default(prog)

    add_data_copy(env, BUILD_DIR, repo_root=REPO_ROOT)
    add_run_target(env, prog)

    exit_if_compiledb_gen_only(env)
"""

import os
import shutil

from SCons.Script import ARGUMENTS
from SCons.Script.SConscript import SConsEnvironment
from SCons.Variables import Variables
from SCons.Variables.BoolVariable import BoolVariable

IMGUI_FILES = [
    'imgui.cpp',
    'imgui_demo.cpp',
    'imgui_draw.cpp',
    'imgui_tables.cpp',
    'imgui_widgets.cpp',
    'backends/imgui_impl_win32.cpp',
    'backends/imgui_impl_dx11.cpp',
]

D3D_LIBS = [
    'user32', 'gdi32', 'ole32', 'kernel32',
    'd3d11', 'dxgi', 'd3dcompiler', 'dxguid',
    'DirectXTK', 'windowscodecs', 'uuid',
]


def make_example_env(repo_root, *, imgui=True, nominmax=True):
    """Environment примера: clang-cl, C++17, пути репозитория, D3D-библиотеки.

    debug=0|1 выбирает Debug/Release LIBPATH DirectXTK. Включает опции
    verbose/compiledb/compiledb_gen_only и compiledb-тул.
    """
    debug = ARGUMENTS.get('debug', '1')

    env = SConsEnvironment(ENV=os.environ, TARGET_ARCH='x86_64')
    env['CC'] = 'clang-cl'
    env['CXX'] = 'clang-cl'

    cpppath = [
        os.path.join(repo_root, 'src'),
        os.path.join(repo_root, 'include'),
        os.path.join(repo_root, 'thirdparty', 'directxtk', 'include'),
    ]
    if imgui:
        imgui_root = os.path.join(repo_root, 'thirdparty', 'imgui')
        cpppath += [imgui_root, os.path.join(imgui_root, 'backends')]
    env.Append(CPPPATH=cpppath)

    defines = ['UNICODE', '_UNICODE']
    if nominmax:
        defines.append('NOMINMAX')
    env.Append(CPPDEFINES=defines)
    env.Append(CCFLAGS=['/EHsc', '/std:c++17'])
    if debug == '1':
        env.Append(CCFLAGS=['/Od', '/Zi', '/MDd'], CPPDEFINES=['_DEBUG'])
    else:
        env.Append(CCFLAGS=['/O2', '/MD'], CPPDEFINES=['NDEBUG'])

    dxtk_config = 'Debug' if debug == '1' else 'Release'
    env.Append(LIBPATH=[os.path.join(
        repo_root, 'thirdparty', 'directxtk', 'native', 'lib', 'x64', dxtk_config)])
    env.Append(LIBS=D3D_LIBS)

    opts = Variables([], ARGUMENTS)
    opts.Add(BoolVariable("verbose", "Verbose output", False))
    opts.Add(BoolVariable("compiledb", "Generate compilation DB", False))
    opts.Add(BoolVariable("compiledb_gen_only", "Exit after DB gen", False))
    opts.Update(env)

    if env["compiledb"]:
        env.Tool("compilation_db")
        compiledb = env.CompilationDatabase(target="compile_commands.json")
        env.Default(compiledb)
        if not env["verbose"]:
            env["COMPILATIONDB_COMSTR"] = "$GENCOMSTR"

    return env


def exit_if_compiledb_gen_only(env):
    """Последняя строка SConstruct: выход после генерации compile_commands.json."""
    if env["compiledb"] and env["compiledb_gen_only"]:
        from SCons.Tool.compilation_db import write_compilation_db
        write_compilation_db([env.File("compile_commands.json")], [], env)
        env.Exit()


def collect_framework_objects(env, repo_root, build_dir, exclude=('main', 'shared')):
    """Объекты всех .cpp фреймворка (<repo>/src), кроме указанных папок."""
    objects = []
    src_root = os.path.join(repo_root, 'src')
    for root, dirs, files in os.walk(src_root):
        dirs[:] = [d for d in dirs if d not in exclude]
        dirs.sort()
        for f in files:
            if f.endswith('.cpp'):
                src = os.path.join(root, f)
                rel_base = os.path.splitext(os.path.relpath(src, src_root))[0]
                objects.append(env.Object(
                    target=os.path.join(build_dir, rel_base + '.obj'),
                    source=src,
                ))
    return objects


def collect_imgui_objects(env, repo_root, build_dir):
    """Объекты imgui + бэкендов (win32, dx11) из <repo>/thirdparty/imgui."""
    imgui_root = os.path.join(repo_root, 'thirdparty', 'imgui')
    objects = []
    for f in IMGUI_FILES:
        rel_base = os.path.splitext(f.replace('/', '_'))[0]
        objects.append(env.Object(
            target=os.path.join(build_dir, 'imgui', rel_base + '.obj'),
            source=os.path.join(imgui_root, f),
        ))
    return objects


def collect_tree_sources(env, build_dir, src_dir='src'):
    """Все .cpp из локального дерева src/ как File-ноды в build_dir."""
    sources = []
    src_base = os.path.abspath(src_dir)
    for root, dirs, files in os.walk(src_base):
        dirs.sort()
        for f in files:
            if f.endswith('.cpp'):
                rel = os.path.relpath(os.path.join(root, f), src_base)
                sources.append(env.File(os.path.join(build_dir, rel)))
    return sources


def add_data_copy(env, build_dir, data_dir='data', repo_root=None):
    """Merge shared and local assets into build_dir/data.

    Local data is optional. Local files override shared files, including files
    in nested directories.
    """
    source_roots = []
    local_data = os.path.abspath(data_dir)
    if repo_root:
        shared_data = os.path.join(repo_root, 'data')
        if os.path.isdir(shared_data) and os.path.abspath(shared_data) != local_data:
            source_roots.append(shared_data)
    if os.path.isdir(local_data):
        source_roots.append(local_data)

    # Track files, not only directory mtimes. SCons otherwise misses changes
    # below an existing data/ directory.
    source_files = []
    for root in source_roots:
        for current, dirs, files in os.walk(root):
            dirs.sort()
            for name in sorted(files):
                source_files.append(os.path.join(current, name))

    def _copy_data_with_roots(target, source, env):
        dst = str(target[0])
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        os.makedirs(dst, exist_ok=True)
        for root in source_roots:
            shutil.copytree(root, dst, dirs_exist_ok=True)

    data_copy = env.Command(os.path.join(build_dir, data_dir), source_files,
                            _copy_data_with_roots)
    # Directory targets do not reliably capture nested-file changes on all
    # SCons/Windows combinations; copying is cheap and deterministic.
    env.AlwaysBuild(data_copy)
    env.Default(data_copy)
    return data_copy


def add_run_target(env, prog, deps=()):
    """Alias 'run': сборка prog (если требуется) + запуск exe.

    AlwaysBuild — запуск каждый раз, даже если пересборки не было.
    Запуск синхронный: терминал занят, пока приложение открыто (виден stdout).
    """
    marker = os.path.join(str(prog[0].dir), '.run')
    run = env.Command(marker, [prog] + list(deps), [f'"{prog[0].abspath}"'])
    env.AlwaysBuild(run)
    env.Alias('run', run)
    return run
