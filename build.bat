@echo off
chcp 65001 > nul
echo ====================================
echo  Сборка ADB Driver for 1C
echo ====================================
echo.

set SOLUTION_DIR=%~dp0

REM Проверка наличия cmake
where cmake > nul 2>&1
if errorlevel 1 (
    echo [ОШИБКА] cmake не найден в PATH
    echo.
    echo Добавьте путь:
    echo   C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
    echo.
    echo В системную переменную PATH (sysdm.cpl).
    pause
    exit /b 1
)

REM Проверка наличия MSBuild
where msbuild > nul 2>&1
if errorlevel 1 (
    echo [ОШИБКА] MSBuild не найден в PATH
    echo.
    echo Добавьте путь:
    echo   C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin
    echo.
    echo В системную переменную PATH (sysdm.cpl).
    pause
    exit /b 1
)

echo [1/3] Генерация проектов CMake...
cmake "%SOLUTION_DIR%" -G "Visual Studio 17 2022" -A x64 -B "%SOLUTION_DIR%build"
if errorlevel 1 (
    echo [ОШИБКА] Ошибка генерации CMake
    pause
    exit /b 1
)
echo    [OK] Проекты сгенерированы
echo.

echo [2/3] Сборка Win64 (Release)...
msbuild "%SOLUTION_DIR%build\ADBFileDriver.sln" /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
if errorlevel 1 (
    echo [ОШИБКА] Ошибка сборки Win64
    pause
    exit /b 1
)
echo    [OK] Win64 собрана успешно
echo.

echo [3/3] Сборка Win32 (Release)...
msbuild "%SOLUTION_DIR%build\ADBFileDriver.sln" /p:Configuration=Release /p:Platform=Win32 /verbosity:minimal
if errorlevel 1 (
    echo [ОШИБКА] Ошибка сборки Win32
    pause
    exit /b 1
)
echo    [OK] Win32 собрана успешно
echo.

echo ====================================
echo  Сборка завершена успешно!
echo  DLL: 1C\Release\Win64\Release\ADBFileDriver_Win64.dll
echo  DLL: 1C\Release\Win32\Release\ADBFileDriver_Win32.dll
echo ====================================
pause