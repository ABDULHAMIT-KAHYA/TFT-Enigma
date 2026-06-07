$ErrorActionPreference = 'Stop'

$files = Get-ChildItem -Path . -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }

g++ -std=c++17 -O2 @files -I../include -o engine.exe
