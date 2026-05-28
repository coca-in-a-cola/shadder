from scons.utility.scons_hints import *
import os
import urllib.request
import zipfile

BUILD_DIR = 'build'
PROGRAMM_NAME = 'SuperShadder'

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

# Single include root — all headers are addressed as "module/file.h"
env.Append(CPPPATH=[
    '#/src',
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

# Kick off the recursive module walk starting from build/SCsub
# (VariantDir maps build/ -> src/, so build/SCsub is src/SCsub)
SConscript(f'{BUILD_DIR}/SCsub', exports={'env': env})

# ---------------------------------------------------------
# 4. FINAL PROGRAM
# ---------------------------------------------------------
prog = env.Program(
    target=f'{BUILD_DIR}/{PROGRAMM_NAME}',
    source=env.module_sources,
)

# Convenience alias
Default(prog)
