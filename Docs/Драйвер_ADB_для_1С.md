# Драйвер ADB для 1С — Журнал изменений

## 30.06.2026 — Исправление архитектуры Win64 DLL

### КРИТИЧЕСКАЯ ПРОБЛЕМА: Win64 DLL была скомпилирована как 32-битная!

**Файл:** `build/ADBFileDriver_Win64.vcxproj`
**Проблема:** В vcxproj файле для ADBFileDriver_Win64 была указана архитектура `/machine:X86` вместо `/machine:X64`. Это приводило к тому, что "Win64" DLL была на самом деле 32-битной, и 64-битная 1С не могла её загрузить.

**Причина:** CMake сгенерировал vcxproj с неправильной платформой — только `win32`, без `x64`.

**Исправление:**
1. Очистил кэш CMake (`CMakeCache.txt` и `CMakeFiles`)
2. Пересоздал проект с правильной архитектурой: `-A x64`
3. Собрал с платформой `x64`: `/p:Platform=x64`

**Результат:**
- До: `14C machine (x86)` — 32-битная
- После: `8664 machine (x64)` — 64-битная ✅

### Предыдущие исправления (29.06.2026):

#### 1. GetClassNames() — отсутствие двойного \0
**Исправление:** Заменил на массив символов с явным двойным `\0` в конце.

#### 2. GetClassObject() проверяла имя класса
**Исправление:** Убрал проверку `_wcsicmp(clsName, L"ADBFileDriver")` — теперь просто `! *pIntf`.

#### 3. Несоответствие типов `std::string::npos` и `std::wstring::find()`
**Исправление:** Заменил `std::string::npos` на `std::wstring::npos`.

#### 4. setWStringToTVariant не инициализирует tVariant
**Исправление:** Добавлена инициализация через `tVarInit(dest)`.

#### 5. Неиспользуемая функция WideToAnsi
**Исправление:** Удалил объявление функции из заголовочного файла.

### Результат сборки:
- **1C/Release/Win64/Release/ADBFileDriver_Win64.dll** — 64-битная (8664 machine (x64)) ✅
- **1C/Release/Win32/Release/ADBFileDriver_Win32.dll** — 32-битная ✅
- Ошибок компиляции: 0
- Предупреждений: 4 (C4305 — усечение VARIANT_BOOL в bool, не критично)

### Инструкция по сборке:
```powershell
# 1. Пересоздать CMake проект с x64:
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" "J:\Visual studio prog\ADB Driver for Android" -G "Visual Studio 17 2022" -A x64 -B "J:\Visual studio prog\ADB Driver for Android\build"

# 2. Собрать:
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" build\ADBFileDriver.sln /p:Configuration=Release /p:Platform=x64
```

### Попробуйте подключить DLL в 1С:
```bsl
// Для 64-битной 1С:
Результат = ПодключитьВнешнююКомпоненту(
    "J:\Visual studio prog\ADB Driver for Android\1C\Release\Win64\Release\ADBFileDriver_Win64.dll",
    "ADBFileDriver",
    ТипВнешнейКомпоненты.Native
);