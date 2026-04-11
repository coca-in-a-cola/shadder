"""
Generate compile_commands.json for VSCode/clang integration
Run: python generate_compile_db.py
"""
import subprocess
import json
import os
import re

def generate_compile_commands():
    """Run SCons with --debug=explain to extract compile commands"""
    print("Generating compile_commands.json...")
    
    # Clean first
    subprocess.run(["python", "-m", "SCons", "-c"], capture_output=True)
    
    # Build with SCons, capturing output
    result = subprocess.run(
        ["python", "-m", "SCons", "--debug=explain"],
        capture_output=True,
        text=True,
        cwd=os.getcwd()
    )
    
    # This is a simplified approach - for better results, use Bear or similar tools
    # For now, we'll create a manual compile_commands.json based on SConstruct
    
    dxtk_path = os.path.join(os.getcwd(), 'thirdparty', 'directxtk', 'include')
    src_path = os.path.join(os.getcwd(), 'src')
    core_path = os.path.join(os.getcwd(), 'src', 'core')
    
    compile_commands = []
    
    # Find all source files
    for root, dirs, files in os.walk(os.path.join(os.getcwd(), 'src')):
        for file in files:
            if file.endswith('.cpp'):
                filepath = os.path.join(root, file)
                rel_path = os.path.relpath(filepath, os.getcwd())
                
                command = (
                    f"clang-cl.exe "
                    f"-I\"{src_path}\" "
                    f"-I\"{core_path}\" "
                    f"-I\"{dxtk_path}\" "
                    f"-std=c++17 "
                    f"-fms-compatibility "
                    f"-fms-extensions "
                    f"-c {rel_path} "
                    f"-o bin\\{os.path.splitext(file)[0]}.o"
                )
                
                compile_commands.append({
                    "directory": os.getcwd(),
                    "command": command,
                    "file": rel_path
                })
    
    output_path = os.path.join(os.getcwd(), 'compile_commands.json')
    with open(output_path, 'w') as f:
        json.dump(compile_commands, f, indent=2)
    
    print(f"Generated {len(compile_commands)} compile commands at: {output_path}")
    return compile_commands

if __name__ == "__main__":
    generate_compile_commands()
