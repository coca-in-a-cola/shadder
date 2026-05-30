from scons.utility.scons_hints import *
import os
import sys
from pathlib import Path
import urllib.request
import zipfile
import shutil

BUILD_DIR = 'bin'
PROGRAMM_NAME = 'SuperShadder'
DATA_DIR = 'data'

# ---------------------------------------------------------
# 0. BUILD OPTIONS (The Godot Way)
# ---------------------------------------------------------
customs = ["custom.py"]

profile = ARGUMENTS.get("profile", "")
if profile:
    if os.path.isfile(profile):
        customs.append(profile)
    elif os.path.isfile(profile + ".py"):
        customs.append(profile + ".py")

opts = Variables(customs, ARGUMENTS)

opts.Add(BoolVariable("verbose", "Enable verbose output for the compilation", False))
opts.Add(BoolVariable("compiledb", "Generate compilation DB (`compile_commands.json`) for external tools", False))
opts.Add(BoolVariable("compiledb_gen_only", "Exit after building the compilation database", False))

# ---------------------------------------------------------
# 1. THIRDPARTY MANAGER (The Godot Way)
# ---------------------------------------------------------
def fetch_directxtk():
    thirdparty_dir = os.path.join(Dir('#').abspath, 'thirdparty')
    dxtk_dir = os.path.join(thirdparty_dir, 'directxtk')

    if os.path.exists(dxtk_dir):
        return dxtk_dir

    print("--> DirectXTK not found. Downloading to thirdparty/...")
    os.makedirs(thirdparty_dir, exist_ok=True)

    url = "https://www.nuget.org/api/v2/package/directxtk_desktop_win10/2024.2.22.1"
    zip_path = os.path.join(thirdparty_dir, "dxtk.zip")

    urllib.request.urlretrieve(url, zip_path)

    print("--> Extracting DirectXTK...")
    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        zip_ref.extractall(dxtk_dir)

    os.remove(zip_path)
    print("--> Done!")

    return dxtk_dir

# Fetch dependencies before building
dxtk_base = fetch_directxtk()

# ---------------------------------------------------------
# 2. STANDARD BUILD ENVIRONMENT
# ---------------------------------------------------------
# Map source tree into build dir (no duplication of files)
VariantDir(BUILD_DIR, 'src', duplicate=False)

debug = ARGUMENTS.get('debug', '1')

env = Environment(
    ENV=os.environ,
    TARGET_ARCH='x86_64',
)

# ---------------------------------------------------------
# 2a. WINDOWS TOOLCHAIN  (Platform-Specific)
# ---------------------------------------------------------
# Use clang-cl instead of MSVC cl.exe so that the generated
# compile_commands.json is consumable by clangd / clang-tidy.
# Other platforms would configure gcc/clang here.
env['CC'] = 'clang-cl'
env['CXX'] = 'clang-cl'
# Linker: keep the MSVC link.exe default on Windows.

# Single include root — all headers are addressed as "module/file.h"
env.Append(CPPPATH=[
    '#/src',
    '#/include',
    '#/thirdparty/directxtk/include',
])

# UNICODE support (needed for wide-string Win32 APIs and D3DCompileFromFile)
env.Append(CPPDEFINES=['UNICODE', '_UNICODE'])

# Enable C++ exception handling and C++17
env.Append(CCFLAGS=['/EHsc', '/std:c++17'])

# Library paths
if debug == '1':
    env.Append(LIBPATH=['#/thirdparty/directxtk/native/lib/x64/Debug'])
else:
    env.Append(LIBPATH=['#/thirdparty/directxtk/native/lib/x64/Release'])

# System and D3D libraries
env.Append(LIBS=[
    'user32', 'gdi32', 'ole32', 'kernel32',
    'd3d11', 'dxgi', 'd3dcompiler', 'dxguid',
])

# ---------------------------------------------------------
# 3. MODULE SOURCE COLLECTION (Godot-style)
# ---------------------------------------------------------
# Helper method mirroring Godot's methods.add_source_files:
#   env.add_source_files(env.module_sources, "*.cpp")
def add_source_files(env, sources, files):
    """Append compiled objects for every .cpp matching *files* glob to *sources*."""
    if isinstance(files, str):
        # Glob is evaluated relative to the SCsub's directory via VariantDir mapping
        files = env.Glob(files)
    sources.extend(files)

env.AddMethod(add_source_files, "add_source_files")

# Shared list that every SCsub will append to
env.module_sources = []

# Update the environment to have all above options defined
# in following code (especially platform and custom_modules).
opts.Update(env)

if env["compiledb"]:
    env.Tool("compilation_db")
    compiledb = env.CompilationDatabase(target="compile_commands.json")
    env.NoCache(compiledb)
    env.Default(compiledb)          # ensure it actually builds
    if not env["verbose"]:
        env["COMPILATIONDB_COMSTR"] = "$GENCOMSTR"

# Kick off the recursive module walk starting from build/SCsub
# (VariantDir maps build/ -> src/, so build/SCsub is src/SCsub)
SConscript(f'{BUILD_DIR}/SCsub', exports={'env': env})

# ---------------------------------------------------------
# 3b. GENERATE NAMESPACE WRAPPER HEADER
# ---------------------------------------------------------
sys.path.insert(0, Dir('#').abspath)
from scons.utility.generate_wrapper import generate_wrapper

src_root = os.path.join(Dir('#').abspath, 'src')
framework_headers = []
for root, dirs, files in os.walk(src_root):
    for f in files:
        if f.endswith('.h'):
            framework_headers.append(os.path.join(root, f))

def _gen_wrapper_action(target, source, env):
    out_path = str(target[0])
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    header_paths = [str(s) for s in source if str(s).endswith('.h')]
    generate_wrapper(
        header_paths, Path(out_path), 'shadder',
        root_header=Path(Dir('#').abspath, 'src', 'index.h'),
        include_dirs=[Path(Dir('#').abspath, 'src'), Path(Dir('#').abspath, 'include')],
    )

wrapper_cmd = env.Command(
    target='include/shadder.hpp',
    source=framework_headers + ['scons/utility/generate_wrapper.py'],
    action=_gen_wrapper_action,
)
env.Depends(env.module_sources, wrapper_cmd)

# ---------------------------------------------------------
# 4. FINAL PROGRAM
# ---------------------------------------------------------
prog = env.Program(
    target=f'{BUILD_DIR}/{PROGRAMM_NAME}',
    source=env.module_sources,
)

# Convenience alias
Default(prog)

# ---------------------------------------------------------
# 5. COPY DATA ASSETS
# ---------------------------------------------------------
def _copy_data(target, source, env):
    dst = str(target[0])
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(str(source[0]), dst)

data_copy = env.Command(
    os.path.join(BUILD_DIR, DATA_DIR),
    DATA_DIR,
    _copy_data,
)
Default(data_copy)

# After the build graph is fully populated, write the compilation DB and exit
# if we only want the DB without compiling.
if env["compiledb"] and env["compiledb_gen_only"]:
    from SCons.Tool.compilation_db import write_compilation_db
    write_compilation_db([env.File("compile_commands.json")], [], env)
    env.Exit()
