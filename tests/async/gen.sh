#!/bin/bash

rm -rf compile_commands.json
cat << EOF > compile_commands.json
[
  {
EOF
cp async-a.hpp ./files/async-a.hpp
for i in {1..98}; do
    cp async-a.cpp files/async-a-$i.cpp
    echo '    "directory": "./files",' >> compile_commands.json
    echo '    "command": "\"C:/Users/Vinnie/AppData/Roaming/ClangPowerTools/LLVM_Lite/Bin/clang++.exe\" -x c++ files/async-a-'$i'.cpp",' >> compile_commands.json
    echo '    "file": "files/async-a-'$i'.cpp"' >> compile_commands.json
    echo '  },' >> compile_commands.json
    echo '  {' >> compile_commands.json
done
cp async-a.cpp files/async-a-99.cpp
echo '    "directory": "./files",' >> compile_commands.json
echo '    "command": "\"C:/Users/Vinnie/AppData/Roaming/ClangPowerTools/LLVM_Lite/Bin/clang++.exe\" -x c++ files/async-a-99.cpp",' >> compile_commands.json
echo '    "file": "files/async-a-99.cpp"' >> compile_commands.json
echo '  }' >> compile_commands.json
echo ']' >> compile_commands.json
