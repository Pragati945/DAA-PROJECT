@echo off
echo ========================================
echo Building DAA PBL Project
echo ========================================

set MYSQL_INC=c:\Users\Pragati\Downloads\mysql-connector-c-6.1.11-win32\mysql-connector-c-6.1.11-win32\include
set MYSQL_LIB=c:\Users\Pragati\Downloads\mysql-connector-c-6.1.11-win32\mysql-connector-c-6.1.11-win32\lib

set SRC_FILES=database\database.cpp src\Dashboard.cpp src\dijkstra.cpp src\graph.cpp src\graphcoloring.cpp src\main.cpp

echo Compiling...
g++ -std=c++17 ^
    -Iinclude ^
    -Idatabase ^
    -I%MYSQL_INC% ^
    %SRC_FILES% ^
    "%MYSQL_LIB%\libmysql.lib" ^
    -o daa_pbl.exe

if errorlevel 1 (
    echo.
    echo BUILD FAILED!
    pause
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.
echo Running program...
echo.
daa_pbl.exe
pause