from scons.utility.scons_hints import *
import os
import urllib.request
import zipfile

# ---------------------------------------------------------
# 1. THIRDPARTY MANAGER (The Godot Way)
# ---------------------------------------------------------
def fetch_directxtk():
    # Dir('#') gets the absolute path to the root of the SConstruct file
    thirdparty_dir = os.path.join(Dir('#').abspath, 'thirdparty')
    dxtk_dir = os.path.join(thirdparty_dir, 'directxtk')
    
    # Check if we already downloaded it
    if os.path.exists(dxtk_dir):
        return dxtk_dir

    print("--> DirectXTK not found. Downloading to thirdparty/...")
    os.makedirs(thirdparty_dir, exist_ok=True)
    
    # Download the official Microsoft DirectXTK NuGet package (it's just a zip file)
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
VariantDir('bin', 'src', duplicate=False)

debug = ARGUMENTS.get('debug', '1')

env = Environment(
    ENV=os.environ,
    TARGET_ARCH='x86_64',
)

# NuGet packages structure their C++ files under the 'native' folder
dxtk_include = os.path.join(dxtk_base, 'include')

# The '#' tells SCons to look at the project root, regardless of VariantDir
env.Append(CPPPATH=[
    'src', 
    'src/core', 
    '#/thirdparty/directxtk/include' 
])

# Do the same for your LIBPATH just to be safe:
if debug == '1':
    env.Append(LIBPATH=['#/thirdparty/directxtk/lib/x64/Debug']) # Adjust 'lib...' to your actual path
else:
    env.Append(LIBPATH=['#/thirdparty/directxtk/lib/x64/Release'])

sources = ['bin/main.cpp'] + Glob('bin/core/*.cpp')
prog = env.Program(target='bin/SuperShadder', source=sources)

env.Command('run', prog, os.path.abspath('bin/SuperShadder.exe'))