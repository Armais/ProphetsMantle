@echo off
title MANTLE - Build
echo ============================================
echo   MANTLE - Compilacion
echo ============================================
echo.

set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

:: Configurar si no existe el directorio build
if not exist "build\CMakeCache.txt" (
    echo [1/2] Configurando CMake...
    %CMAKE% -B build -S .
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo ERROR: Fallo la configuracion de CMake
        pause
        exit /b 1
    )
    echo.
)

:: Compilar
echo [2/2] Compilando...
%CMAKE% --build build --config Release
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================
    echo   Build OK - Lanzando MANTLE.exe...
    echo ============================================
    echo.
    start "" "MANTLE.exe"
) else (
    echo.
    echo ============================================
    echo   ERROR: Fallo la compilacion
    echo ============================================
    echo.
    pause
)
