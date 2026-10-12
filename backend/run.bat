@echo off
setlocal
echo Starting AI Early Health Detection Center Server...
if not exist server.exe (
    echo server.exe not found! Building first...
    call build.bat
)
server.exe
endlocal
