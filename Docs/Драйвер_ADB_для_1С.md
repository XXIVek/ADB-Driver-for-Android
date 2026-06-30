# Драйвер USB для 1С — Журнал изменений

## 01.07.2026 — Сборка проекта успешно выполнена

### Результат сборки:
- **Генерация CMake**: ✅ cmake 3.31.6, Visual Studio 17 2022
- **Сборка Win64**: ✅ `1C\Release\Win64\Release\ADBFileDriver_Win64.dll` (61440 байт)
- **Сборка Win32**: ✅ `1C\Release\Win32\Release\ADBFileDriver_Win32.dll` (61440 байт)

### Команды для сборки:
```bash
# Генерация проектов
cmake "j:\Visual studio prog\ADB Driver for Android" -G "Visual Studio 17 2022" -A x64 -B build

# Сборка
msbuild build\ADBFileDriver.sln /p:Configuration=Release /p:Platform=x64
msbuild build\ADBFileDriver.sln /p:Configuration=Release /p:Platform=Win32
```

### ⚠️ Исправление 1: Зависимости AdbWinUsbApi.dll
**Проблема**: Компонента не загружается — "нет зависимости" (missing dependency).

**Решение**: Скопировать зависимости в папку с DLL:
```powershell
# Win64
Copy-Item "adb\AdbWinUsbApi.dll" "1C\Release\Win64\Release\"
Copy-Item "adb\AdbWinApi.dll" "1C\Release\Win64\Release\"

# Win32
Copy-Item "adb\AdbWinUsbApi.dll" "1C\Release\Win32\Release\"
Copy-Item "adb\AdbWinApi.dll" "1C\Release\Win32\Release\"
```

**Скопированные файлы:**
- `AdbWinApi.dll` (108544 байт, 05.07.2018)
- `AdbWinUsbApi.dll` (65024 байт, 05.07.2018)

### ⚠️ Исправление 2: Имя класса в GetClassNames()
**Проблема**: `ПодключитьВнешнююКомпоненту` вернула Ложь — 1С не может найти класс.

**Причина**: `GetClassNames()` возвращал `USBDeviceDriver`, но `GetClassObject()` проверял `ADBFileDriver`.

**Решение**: Синхронизировать имена:
```cpp
// GetClassNames() должен возвращать то же имя, что проверяет GetClassObject()
static WCHAR_T names[] = { L'A', L'D', L'B', L'F', L'i', L'l', L'e', L'D', L'r', L'i', L'v', L'e', L'r', L'\0', L'\0' };
```

---


## ⚠️ ВАЖНО: Сборка проекта

**Не нажимайте кнопку "Сборка" в VSCode!** Она может собрать DLL для неправильной архитектуры.

**Правильный способ сборки:**
```
build.bat
```
или вручную:
```
cmake -G "Visual Studio 17 2022" -A x64 -B build
msbuild build\ADBFileDriver.sln /p:Configuration=Release /p:Platform=x64
msbuild build\ADBFileDriver.sln /p:Configuration=Release /p:Platform=Win32
```

## 30.06.2026 — Версия 2.0.0.0 — Переход на прямую работу с USB

### Крупные изменения:

**Полностью переписана архитектура компоненты:**

1. **Удалена зависимость от adb.exe** — компонента больше не использует процесс ADB
2. **Прямая работа с USB устройствами** через AdbWinUsbApi.dll
3. **Новая реализация** на основе `adb_api.h` из `include/adb-win64-android-p-preview-4/prebuilt/`

### Новая структура файлов:

```
ADB Driver for Android/
├── ADBFileDriver/
│   ├── ADBFileDriver.h      — заголовочный файл компоненты
│   ├── ADBFileDriver.cpp    — основная реализация (переписана)
│   ├── AdbWinHelper.h       — НОВЫЙ: обертка над AdbWinUsbApi.dll
│   ├── AdbWinHelper.cpp     — НОВЫЙ: реализация обертки
│   ├── ADBFileDriver.def    — экспорт символов
│   └── CMakeLists.txt       — обновлен для новой сборки
├── include/
│   └── adb-win64-android-p-preview-4/
│       ├── prebuilt/
│       │   ├── adb_api.h
│       │   ├── AdbWinApi.dll
│       │   └── AdbWinUsbApi.dll
│       └── ...
└── 1C/
    └── Release/
        ├── Win32/
        │   └── ADBFileDriver_Win32.dll
        └── Win64/
            └── ADBFileDriver_Win64.dll
```

### API компоненты (версия 2.0.0.0):

#### Свойства (3):
| Имя (RU) | Имя (EN) | Тип | Описание |
|----------|----------|-----|----------|
| СтатусПодключения | Status | Строка | Текущий статус |
| СписокУстройств | DeviceList | Строка (JSON) | JSON список USB устройств |
| СостояниеUSB | USBState | Bool | true = USB инициализирован |

#### Методы (5):
| Имя (RU) | Имя (EN) | Параметры | Возврат | Описание |
|----------|----------|-----------|---------|----------|
| ПодключитьУстройство | Connect | нет | Истина/Ложь | Подключение к первому USB устройству |
| ОтключитьУстройство | Disconnect | нет | Истина/Ложь | Отключение от устройства |
| ЗаписатьНаУстройство | PushFile | (ПутьНаПК, Данные) | Истина/Ложь | Запись данных на устройство |
| ПрочитатьСУстройства | PullText | (Размер) | Строка | Чтение данных с устройства |
| ПолучитьСписокУстройств | GetDeviceList | нет | Строка (JSON) | Список USB устройств |

### Требования к окружению:

1. **AdbWinUsbApi.dll** должна быть доступна в одной из следующих директорий:
   - Директория, где находится DLL компоненты
   - `adb/` в директории компоненты
   - Системная PATH

2. **Драйверы USB** для Android устройства должны быть установлены

3. **GUID устройства**: `{F72FE0D4-CBCB-407d-8814-9ED673D0DD6B}`

### Пример подключения в 1С (версия 2.0.0.0):
```bsl
// Для 64-битной 1С:
Результат = ПодключитьВнешнююКомпоненту(
    "J:\Visual studio prog\ADB Driver for Android\1C\Release\Win64\Release\ADBFileDriver_Win64.dll",
    "ADBFileDriver",
    ТипВнешнейКомпоненты.Native
);

// Проверка состояния USB
Если Результат.СостояниеUSB = Истина Тогда
    Сообщить("USB инициализирован");
    Список = Результат.ПолучитьСписокУстройств();
    
    // Подключение к первому устройству
    Если Результат.ПодключитьУстройство() Тогда
        Сообщить("Подключено к устройству");
    КонецЕсли;
Иначе
    Сообщить("USB не инициализирован! Проверьте наличие AdbWinUsbApi.dll");
КонецЕсли;
```

### Известные ограничения:
- Только USB подключение (не tcp/ip)
- Требуется установленный драйвер USB для Android
- Работает только на Windows

---

## 30.06.2026 — Фиксированный путь ADB (временно)

### Изменения в поиске adb.exe:

**Приоритет поиска adb.exe (новый порядок):**

1. **`C:\temp\ADB\adb.exe`** — [ВРЕМЕННО] фиксированный путь (первый приоритет)
2. Директория, где находится DLL файла
3. Системная переменная PATH
4. Стандартные пути Android SDK (Program Files)
5. Пользовательский каталог `~\AppData\Local\Android\Sdk\`
6. Реестр `HKLM\SOFTWARE\Android SDK`

---

## 30.06.2026 — Очистка Git репозитория

### Выполненные изменения:

- **Удалены из трекинга:** 64 файла в папке `build/` (CMake-генерируемые файлы)
- **Обновлен `.gitignore`:** добавлены правила для CMake, временных файлов C++ и логов
- **Итого файлов в репозитории:** 64 → 57 (удалено 7 файлов)
- **Удалено строк:** 9366 строк из build/ файлов
- **Добавлены файлы:** `build.bat`, `.clinerules/Для сборки.md`

### Обновленный `.gitignore`:
```
build/                    # CMake-генерируемые файлы
CMakeCache.txt           # Кэш CMake
cmake_install.cmake      # CMake скрипты
linkerrors.txt           # Логи ошибок линковщика
CMakeFiles/              # Промежуточные файлы CMake
*.ilk, *.pdb, *.obj      # Временные файлы C++
```


## 30.06.2026 — Версия 1.0.0.4

### Скомпилированные DLL:
- **1C/Release/Win64/Release/ADBFileDriver_Win64.dll** — собрано ✅
- **1C/Release/Win32/Release/ADBFileDriver_Win32.dll** — собрано ✅

### Добавлено новое свойство:

**СостояниеADBСервера** (ADBServerState) — тип Bool
- `true` — ADB сервер запущен и работает
- `false` — ADB сервер не запущен или ошибка

### Итоговый сценарий работы (версия 1.0.0.4):

```
1. Init() → adb start-server → CheckADBServer()
2. СостояниеADBСервера → true/false
3. СписокУстройств → если сервер не запущен → []
4. ПодключитьУстройство() → adb devices
5. Работа с файлами
6. ОтключитьУстройство()
```

### Подключение в 1С (версия 1.0.0.4):
```bsl
// Для 64-битной 1С:
Результат = ПодключитьВнешнююКомпоненту(
    "J:\Visual studio prog\ADB Driver for Android\1C\Release\Win64\Release\ADBFileDriver_Win64.dll",
    "ADBFileDriver",
    ТипВнешнейКомпоненты.Native
);

// Проверка состояния ADB сервера
Если Результат.СостояниеADBСервера = Истина Тогда
    Сообщить("ADB сервер работает");
    Список = Результат.СписокУстройств;
Иначе
    Сообщить("ADB сервер не запущен!");
КонецЕсли;