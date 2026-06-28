# Драйвер ADB для 1С:Предприятие 8.3

## Внешняя компонента обмена файлами через USB/ADB

---

## История изменений

### 2026-06-28
- Исправлен CMakeLists.txt: обновлён аргумент `CMAKE_MINIMUM_REQUIRED` с `VERSION 2.6` на `VERSION 3.10...3.31` для устранения предупреждения CMake о совместимости.
- **Сборка и исправление ошибок:**
  - Исправлен `.def` файл: закомментирована нереализованная функция `GetAttachType @5`
  - Добавлена реализация абстрактного метода `SetLocale(const WCHAR_T* loc)` из `LocaleBase`
  - Исправлен конфликт сигнатур `GetClassNames`: изменён возвращаемый тип с `WCHAR_T*` на `const WCHAR_T*` для соответствия `ComponentBase.h`
  - Удалены дублирующие объявления экспортируемых функций из заголовочного файла
  - Исправлено преобразование типов: `ExecuteADBCommand()` возвращает `std::wstring`, код теперь корректно обрабатывает результат без попыток присвоения в `std::string`
  - Исправлены методы `Method_PushFile` и `Method_PullText`: проверка результата теперь использует `std::wstring` и широкие строковые литералы (L"...")
  - Добавлена реализация функции `GetAttachType()` (возвращает 0 — eCanAttachAny)
  - Настроена сборка двух версий: Win32 (i386) и Win64 (x86_64)
  - **Результат сборки:** успешно, созданы две DLL:
    - `1C\Release\Win32\Release\ADBFileDriver_Win32.dll` (для arch="i386")
    - `1C\Release\Win64\Release\ADBFileDriver_Win64.dll` (для arch="x86_64")
  - **Остаются предупреждения:** C4305 (усечение VARIANT_BOOL→bool), C4996 (_wfopen unsafe)

---

## Оглавление

1. [Общие сведения](#1-общие-сведения)
2. [Архитектура](#2-архитектура)
3. [Требования](#3-требования)
4. [Интерфейс компоненты](#4-интерфейс-компоненты)
5. [Примеры подключения из 1С](#5-примеры-подключения-из-1с)
6. [Технология разработки новых драйверов](#6-технология-разработки-новых-драйверов)
7. [Установка и регистрация](#7-установка-и-регистрация)
8. [Техническая документация Native API](#8-техническая-документация-native-api)

---

## 1. Общие сведения

### 1.1 Назначение

Внешняя компонента `ADBFileDriver` предназначена для обмена файлами между компьютером под управлением Windows и устройством на базе Android через USB-кабель с использованием Android Debug Bridge (ADB).

### 1.2 Возможности

- Подключение устройства Android через USB
- Загрузка файлов с ПК на телефон (в папку `/Download/`)
- Скачивание файлов с телефона на ПК
- Прямая запись/чтение JSON-данных
- Прямая запись/чтение CSV-данных
- Управление папками на устройстве
- Получение списка файлов на устройстве

### 1.3 Поддерживаемые платформы

| Платформа | Поддержка |
|-----------|-----------|
| Windows x86_64 | Да |
| Windows x86 | Да |
| Linux x86_64 | Нет |
| Linux ARM64 | Нет |
| macOS | Нет |

### 1.4 Версия компоненты

**Текущая версия:** 1.0.0

---

## 2. Архитектура

### 2.1 Схема взаимодействия

```
┌─────────────────────────────────────────────────────────┐
│                    1С:Предприятие 8.3                    │
│              (собственная разработка)                     │
│                                                          │
│  Драйвер = Новый ЗагрузитьВнешнююКомпоненту(             │
│      "ADBFileDriver.dll",                               │
│      "ADBFileDriver"                                     │
│  );                                                      │
│                                                          │
│  Драйвер.ЗагрузитьНаТелефон("C:\data.json", "/Download/")│
│  Драйвер.СкачатьСТелефона("/Download/file.csv",          │
│      "C:\data\")                                         │
└────────────────────────┬────────────────────────────────┘
                        │ Native API (IComponentBase)
┌────────────────────────▼────────────────────────────────┐
│           ADBFileDriver.dll                            │
│         (Native API компонента)                         │
│                                                          │
│  - IComponentBase (интерфейс компоненты)                │
│  - ILanguageExtender (расширение языка)                 │
│  - ADBManager (управление ADB)                          │
│  - FileManager (обмен файлами)                          │
└────────────────────────┬────────────────────────────────┘
                        │ ADB протокол (USB)
┌────────────────────────▼────────────────────────────────┐
│         Android Debug Bridge (adb.exe)                  │
│                                                          │
│  - adb devices                                          │
│  - adb push <file> <path>                              │
│  - adb pull <path> <file>                              │
│  - adb shell ls <path>                                 │
│  - adb shell mkdir <path>                              │
│  - adb shell rm <path>                                 │
└────────────────────────┬────────────────────────────────┘
                        │ USB
┌────────────────────────▼────────────────────────────────┐
│                   Android телефон                        │
│           /sdcard/Download/                             │
│         (готовое приложение-получатель)                  │
└─────────────────────────────────────────────────────────┘
```

### 2.2 Компоненты драйвера

| Компонента | Описание |
|------------|----------|
| ADBFileDriver.dll | Основная DLL-библиотека (Native API) |
| adb.exe | Исполняемый файл Android Debug Bridge |
| ADBDriver.json | Конфигурационный файл |

---

## 3. Требования

### 3.1 Системные требования

- Операционная система: Windows 10/11 (x86_64)
- 1С:Предприятие 8.3 (любая версия, начиная с 8.3.6)
- Установленные драйверы USB для Android-устройства
- Включённый режим отладки USB на Android-устройстве

### 3.2 Требования к Android-устройству

- Android 5.0 (API 21) и выше
- Включённый режим разработчика
- Включённая отладка по USB
- Подключённое через USB-кабель

### 3.3 Структура папок после установки

```
ADBFileDriver/
├── ADBFileDriver.dll      — внешняя компонента
├── adb.exe                  — Android Debug Bridge
├── ADBManager.exe           — менеджер ADB-подключений
├── ADBDriver.json           — конфигурационный файл
├── docs/
│   ├── Драйвер_ADB_для_1С.md — эта документация
│   └── Технология создания внешних компонент.docx
└── examples/
    └── ПримерПодключения.bsl — пример подключения из 1С
```

---

## 4. Интерфейс компоненты

### 4.1 Регистрация компоненты

**ProgID:** `AddIn.ADBFileDriver`

**Имя расширения встроенного языка:** `ADBFileDriver`

### 4.2 Свойства

#### `СтатусПодключения` (Текст, чтение)

Возвращает текущий статус подключения.

**Возможные значения:**
- `"Не подключено"` — устройство не подключено
- `"Подключение..."` — идёт процесс подключения
- `"Подключено"` — устройство успешно подключено
- `"Ошибка: <описание>"` — ошибка подключения

**Пример:**
```bsl
Сообщить(Драйвер.СтатусПодключения);
```

#### `СписокУстройств` (Текст, чтение)

Возвращает JSON-строку со списком всех подключённых ADB-устройств.

**Структура JSON:**
```json
[
  {"Serial": "ABC123", "Status": "device", "Model": "Samsung Galaxy S21"},
  {"Serial": "DEF456", "Status": "offline"}
]
```

**Пример:**
```bsl
JSONstr = Драйвер.СписокУстройств;
// Парсить через ЗагрузитьИзJSON()
```

### 4.3 Методы

#### `ПодключитьУстройство()`

Подключается к первому доступному Android-устройству со статусом "device".

**Возвращаемое значение:** `Истина` / `Ложь`

**Пример:**
```bsl
Если Драйвер.ПодключитьУстройство() Тогда
    Сообщить("Подключено: " + Драйвер.СтатусПодключения)
Иначе
    Сообщить("Нет подключенных устройств")
КонецЕсли;
```

#### `ОтключитьУстройство()`

Отключает от текущего устройства.

**Возвращаемое значение:** `Истина` / `Ложь`

#### `ЗаписатьНаТелефон(ПутьНаПК, ПутьНаТелефоне)`

Загружает файл с ПК на Android-устройство.

**Параметры:**
- `ПутьНаПК` (Текст) — полный путь к файлу на компьютере
- `ПутьНаТелефоне` (Текст) — путь на Android-устройстве (например, `/Download/`)

**Возвращаемое значение:** `Истина` / `Ложь`

**Пример:**
```bsl
Если Драйвер.ЗаписатьНаТелефон("C:\data.json", "/Download/data.json") Тогда
    Сообщить("Файл выгружен")
КонецЕсли;
```

#### `ПрочитатьСТелефона(ПутьНаТелефоне)`

Скачивает текстовое содержимое файла с Android-устройства.

**Параметры:**
- `ПутьНаТелефоне` (Текст) — путь на Android-устройстве

**Возвращаемое значение:** Текст — содержимое файла или пустая строка при ошибке

**Пример:**
```bsl
Содержимое = Драйвер.ПрочитатьСТелефона("/Download/config.json")
Если НЕ ПустаяСтрока(Содержимое) Тогда
    // обработка содержимого
КонецЕсли;
```

#### `ПолучитьСписокУстройств()`

Возвращает JSON-строку со списком всех ADB-устройств.

**Возвращаемое значение:** Текст — JSON-строка

---

## 5. Примеры подключения из 1С

### 5.1 Базовое подключение

```bsl
// Подключение внешней компоненты
Попытка
    Драйвер = Новый ЗагрузитьВнешнююКомпоненту(
        "C:\ADBFileDriver\ADBFileDriver.dll",
        "ADBFileDriver",
        ТипВнешнейКомпоненты.NativeAPI
    );
Исключение
    Сообщить("Ошибка подключения компоненты: " + ОписаниеОшибки());
    Возврат;
КонецПопытки;

// Проверка подключения
Если НЕ Драйвер.ПодключитьУстройство() Тогда
    Сообщить("Не удалось подключить устройство: " + Драйвер.СтатусПодключения);
    Возврат;
КонецЕсли;

// ... работа с драйвером ...

// Отключение
Драйвер.ОтключитьУстройство();
```

### 5.2 Выгрузка данных на телефон

```bsl
Процедура ВыгрузитьНаТелефон(Компоновщик, ПутьНаТелефоне)
    
    // Подключение
    Драйвер = Новый ЗагрузитьВнешнююКомпоненту(
        "C:\ADBFileDriver\ADBFileDriver.dll",
        "ADBFileDriver",
        ТипВнешнейКомпоненты.NativeAPI
    );
    
    Если НЕ Драйвер.ПодключитьУстройство() Тогда
        Сообщить("Ошибка: " + Драйвер.СтатусПодключения);
        Возврат;
    КонецЕсли;
    
    // Создание данных
    Данные = Новый ТаблицаЗначений;
    Данные.Колонки.Добавить("Номенклатура", "Строка", 200);
    Данные.Колонки.Добавить("Количество", "Число", 15, 2);
    Данные.Колонки.Добавить("Цена", "Число", 15, 2);
    Данные.Колонки.Добавить("Сумма", "Число", 18, 2);
    
    // Заполнение данными из композитора
    Запрос = Новый Запрос;
    Запрос.Текст = Компоновщик.Макет.Запрос.Текст;
    Результат = Запрос.Выполнить();
    
    Выборка = Результат.Выгрузить();
    Для каждого Стр Из Выборка Цикл
        Новая = Данные.Добавить();
        Новая.Номенклатура = Стр.Номенклатура;
        Новая.Количество = Стр.Количество;
        Новая.Цена = Стр.Цена;
        Новая.Сумма = Стр.Сумма;
    КонецЦикла;
    
    // Запись JSON
    Если Драйвер.ЗаписатьНаТелефон(Данные, ПутьНаТелефоне) Тогда
        Сообщить("Данные выгружены на телефон: " + ПутьНаТелефоне);
    Иначе
        Сообщить("Ошибка записи JSON на телефон");
    КонецЕсли;
    
    // Отключение
    Драйвер.ОтключитьУстройство();
    
КонецПроцедуры

// Использование:
ВыгрузитьНаТелефон(КомпоновщикПроекций, "/Download/report.json");
```

### 5.3 Загрузка данных с телефона

```bsl
Процедура ЗагрузитьСТелефона(ПутьНаТелефоне)
    
    // Подключение
    Драйвер = Новый ЗагрузитьВнешнююКомпоненту(
        "C:\ADBFileDriver\ADBFileDriver.dll",
        "ADBFileDriver",
        ТипВнешнейКомпоненты.NativeAPI
    );
    
    Если НЕ Драйвер.ПодключитьУстройство() Тогда
        Сообщить("Ошибка: " + Драйвер.СтатусПодключения);
        Возврат;
    КонецЕсли;
    
    // Чтение JSON
    Данные = Драйвер.ПрочитатьСТелефона(ПутьНаТелефоне);
    
    Если НЕ ПустаяСтрока(Данные) Тогда
        Сообщить("Файл прочитан успешно");
        // Обработка данных...
    Иначе
        Сообщить("Файл пустой или ошибка чтения");
    КонецЕсли;
    
    // Отключение
    Драйвер.ОтключитьУстройство();
    
КонецПроцедуры

// Использование:
ЗагрузитьСТелефона("/Download/upload.csv");
```

---

## 6. Технология разработки новых драйверов

### 6.1 Структура проекта

```
MyDriver/
├── include/
│   ├── ComponentBase.h       — базовый класс компоненты
│   ├── AddInDefBase.h        — интерфейс 1С:Предприятия
│   ├── IMemoryManager.h      — менеджер памяти
│   └── Platform.h            — платформозависимые определения
├── src/
│   ├── Driver.cpp            — основная реализация
│   ├── ADBManager.cpp        — управление ADB
│   └── FileManager.cpp       — работа с файлами
├── CMakeLists.txt            — скрипт сборки
└── README.md                 — документация
```

### 6.2 Базовый шаблон компоненты

#### 6.2.1 Заголовочный файл `Driver.h`

```cpp
#pragma once

#include "ComponentBase.h"
#include "AddInDefBase.h"
#include "IMemoryManager.h"

// Имя расширения встроенного языка
#define EXTENSION_NAME "MyDriver"

// Класс компоненты
class CMyDriver : public IComponentBase
{
private:
    IAddInDefBase* m_pBackConn;
    IMemoryManager* m_pMemMgr;
    bool m_bInitialized;

public:
    CMyDriver();
    virtual ~CMyDriver();

    // IComponentBase
    virtual bool Init(void* Interface) override;
    virtual void Done() override;
    virtual long GetInfo() override;
    virtual bool setMemManager(void* memManager) override;

    // ILanguageExtender (свойства)
    virtual long GetNProps() override;
    virtual long FindProp(const WCHAR_T* wsPropName) override;
    virtual const WCHAR_T* GetPropName(long lPropNum, long lPropAlias) override;
    virtual bool GetPropVal(const long lPropNum, tVariant* pvarPropVal) override;
    virtual bool SetPropVal(const long lPropNum, tVariant* pvarPropVal) override;
    virtual bool IsPropReadable(const long lPropNum) override;
    virtual bool IsPropWritable(const long lPropNum) override;

    // ILanguageExtender (методы)
    virtual long GetNMethods() override;
    virtual long FindMethod(const WCHAR_T* wsMethodName) override;
    virtual const WCHAR_T* GetMethodName(const long lMethodNum, const long lMethodAlias) override;
    virtual long GetNParams(const long lMethodNum) override;
    virtual bool GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant* pvarParamDefValue) override;
    virtual bool HasRetVal(const long lMethodNum) override;
    virtual bool CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray) override;
    virtual bool CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray) override;

    // ILanguageExtender (профиль)
    virtual bool RegisterExtensionAs(WCHAR_T** wsExtName) override;
};
```

#### 6.2.2 Реализация `Driver.cpp`

```cpp
#include "Driver.h"
#include <wchar.h>
#include <string>

// ============================================================
// Экспортируемые функции (должны быть в DLL)
// ============================================================

extern "C" {

// Возвращает список имён объектов компоненты (разделитель: \0)
const WCHAR_T* GetClassNames()
{
    static WCHAR_T names[] = L"MyDriver";
    return names;
}

// Создаёт экземпляр компоненты
long GetClassObject(const WCHAR_T* clsName, IComponentBase** pIntf)
{
    if (_wcsicmp(clsName, L"MyDriver") == 0)
    {
        *pIntf = new CMyDriver();
        return *pIntf ? 1 : 0;
    }
    return 0;
}

// Удаляет экземпляр компоненты
long DestroyObject(IComponentBase** pIntf)
{
    if (pIntf && *pIntf)
    {
        delete *pIntf;
        *pIntf = nullptr;
        return 0;
    }
    return -1;
}

// Устанавливает версию возможностей платформы
AppCapabilities SetPlatformCapabilities(const AppCapabilities capabilities)
{
    return eAppCapabilitiesLast;
}

// Возвращает тип подключения
long GetAttachType()
{
    return eCanAttachAny; // любое подключение
}

} // extern "C"

// ============================================================
// Реализация CMyDriver
// ============================================================

CMyDriver::CMyDriver() : m_pBackConn(nullptr), m_pMemMgr(nullptr), m_bInitialized(false)
{
}

CMyDriver::~CMyDriver()
{
}

bool CMyDriver::Init(void* Interface)
{
    if (Interface == nullptr)
        return false;

    m_pBackConn = static_cast<IAddInDefBase*>(Interface);
    m_bInitialized = true;
    return true;
}

void CMyDriver::Done()
{
    m_pBackConn = nullptr;
    m_pMemMgr = nullptr;
    m_bInitialized = false;
}

long CMyDriver::GetInfo()
{
    // Версия 1.00 = 100
    return 100;
}

bool CMyDriver::setMemManager(void* memManager)
{
    if (memManager == nullptr)
        return false;

    m_pMemMgr = static_cast<IMemoryManager*>(memManager);
    return true;
}

// LocaleBase — реализация
void CMyDriver::SetLocale(const WCHAR_T* loc)
{
    // TODO: реализовать смену локали
}
```

### 6.3 CMakeLists.txt для сборки

```cmake
cmake_minimum_required(VERSION 3.10)
project(ADBFileDriver VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Исходные файлы
set(ADBFileDriver_SRC
    ADBFileDriver/ADBFileDriver.cpp
    ADBFileDriver/ADBFileDriver.h
)

# Definition file для экспорта символов
set(ADBFileDriver_DEF ADBFileDriver.def)

# Заголовочные файлы
include_directories(${CMAKE_SOURCE_DIR}/include)

# Две версии библиотеки: Win32 (i386) и Win64 (x86_64)
if(WIN32)
    # ADBFileDriver_Win32 — для arch="i386"
    add_library(ADBFileDriver_Win32 SHARED ${ADBFileDriver_SRC})
    target_sources(ADBFileDriver_Win32 PRIVATE ${ADBFileDriver_DEF})
    set_target_properties(ADBFileDriver_Win32 PROPERTIES
        OUTPUT_NAME ADBFileDriver_Win32
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win32"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win32"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win32"
    )
    target_compile_options(ADBFileDriver_Win32 PRIVATE /utf-8 /EHsc)
    target_link_libraries(ADBFileDriver_Win32
        ole32.lib oleaut32.lib shlwapi.lib advapi32.lib
    )

    # ADBFileDriver_Win64 — для arch="x86_64"
    add_library(ADBFileDriver_Win64 SHARED ${ADBFileDriver_SRC})
    target_sources(ADBFileDriver_Win64 PRIVATE ${ADBFileDriver_DEF})
    set_target_properties(ADBFileDriver_Win64 PROPERTIES
        OUTPUT_NAME ADBFileDriver_Win64
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win64"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win64"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/1C/Release/Win64"
    )
    target_compile_options(ADBFileDriver_Win64 PRIVATE /utf-8 /EHsc)
    target_link_libraries(ADBFileDriver_Win64
        ole32.lib oleaut32.lib shlwapi.lib advapi32.lib
    )
endif()
```

---

## 7. Сборка проекта

### 7.1 Подготовка (выполняется один раз)

Откройте терминамент VS Code и выполните:

```powershell
# Конфигурация CMake для Visual Studio 2022
cmake -G "Visual Studio 17 2022" -S . -B build
```

### 7.2 Сборка через VS Code Terminal

**Вариант 1: Через команду VS Code CMake**

1. Откройте командную палитру: `Ctrl+Shift+P`
2. Введите: `CMake: Build`
3. Выберите конфигурацию: `Release`

**Вариант 2: Через MSBuild из терминала VS Code**

```powershell
# Сборка обеих версий (Win32 + Win64)
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" build\ALL_BUILD.vcxproj /p:Configuration=Release

# Сборка только Win32 версии
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" build\ADBFileDriver_Win32.vcxproj /p:Configuration=Release

# Сборка только Win64 версии
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" build\ADBFileDriver_Win64.vcxproj /p:Configuration=Release
```

### 7.3 Результаты сборки

| Версия | Архитектура | Путь к DLL |
|--------|-------------|------------|
| Win32 | i386 | `1C\Release\Win32\Release\ADBFileDriver_Win32.dll` |
| Win64 | x86_64 | `1C\Release\Win64\Release\ADBFileDriver_Win64.dll` |

### 7.4 Предупреждения при сборке

При сборке возникают предупреждения (не ошибки):
- **C4305** — усечение `VARIANT_BOOL` в `bool` (не критично)
- **C4996** — использование `_wfopen` вместо `_wfopen_s` (можно отключить через `#define _CRT_SECURE_NO_WARNINGS`)

---

## 8. Установка и регистрация

### 7.1 Размещение файлов

1. Скопируйте `ADBFileDriver.dll` в папку с вашим конфигурационным файлом 1С
2. Убедитесь, что `adb.exe` находится в той же папке или в PATH

### 7.2 Подключение в 1С

```bsl
// Автоматическое подключение
Драйвер = Новый ЗагрузитьВнешнююКомпоненту(
    "ADBFileDriver.dll",
    "ADBFileDriver",
    ТипВнешнейКомпоненты.NativeAPI
);
```

---

## 8. Техническая документация Native API

### 8.1 Интерфейсы

| Интерфейс | Назначение |
|-----------|------------|
| IInitDoneBase | Инициализация/деинициализация |
| ILanguageExtenderBase | Расширение языка (свойства/методы) |
| LocaleBase | Смена локали |
| IComponentBase | Базовый интерфейс компоненты |
| IMemoryManager | Управление памятью |

### 8.2 Типы данных

| Тип | Описание |
|-----|----------|
| WCHAR_T | Символ (1 или 2 байта) |
| tVariant | Вариантное значение |
| tVariant::vt | Тип значения (VTYPE_EMPTY, VTYPE_BOOL, VTYPE_PWSTR, etc.) |

### 8.3 Коды ошибок

| Код | Описание |
|-----|----------|
| ADDIN_E_FAIL | Общая ошибка |
| E_FAIL | Общая ошибка COM |

---

*Документация создана для ADBFileDriver v1.0.0*
        L"Status",
        L"DeviceID",
        L"Version"
    };

    if (lPropNum < 0 || lPropNum > 2)
        return nullptr;

    // Выделяем память через менеджер памяти 1С
    if (m_pMemMgr)
    {
        WCHAR_T* result = static_cast<WCHAR_T*>(m_pMemMgr->AllocMemory((lPropAlias == 1 ? 12 : 8) * sizeof(WCHAR_T)));
        // Копируем имя...
        return result;
    }

    return names[lPropNum];
}

bool CMyDriver::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
    switch (lPropNum)
    {
        case 0: // Status
            pvarPropVal->vt = VTYPE_PWSTR;
            // Записать строку статуса...
            break;
        case 1: // DeviceID
            pvarPropVal->vt = VTYPE_PWSTR;
            // Записать ID устройства...
            break;
        case 2: // Version
            pvarPropVal->vt = VTYPE_PWSTR;
            // Записать версию...
            break;
        default:
            pvarPropVal->vt = VTYPE_EMPTY;
            return false;
    }
    return true;
}

bool CMyDriver::SetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
    if (lPropNum == 1) // DeviceID — только для записи
    {
        // Сохранить идентификатор устройства...
        return true;
    }
    return false;
}

bool CMyDriver::IsPropReadable(const long lPropNum)
{
    return lPropNum >= 0 && lPropNum <= 2;
}

bool CMyDriver::IsPropWritable(const long lPropNum)
{
    return lPropNum == 1; // только DeviceID
}

// ============================================================
// Методы (ILanguageExtender)
// ============================================================

long CMyDriver::GetNMethods()
{
    return 10; // количество методов
}

long CMyDriver::FindMethod(const WCHAR_T* wsMethodName)
{
    static const WCHAR_T* methods[] = {
        L"Connect",
        L"Disconnect",
        L"GetFileList",
        L"UploadToPhone",
        L"DownloadFromPhone",
        L"WriteJSON",
        L"ReadJSON",
        L"WriteCSV",
        L"ReadCSV",
        L"GetInfo"
    };

    for (int i = 0; i < 10; i++)
    {
        if (_wcsicmp(wsMethodName, methods[i]) == 0)
            return i;
    }
    return -1;
}

const WCHAR_T* CMyDriver::GetMethodName(const long lMethodNum, const long lMethodAlias)
{
    static const WCHAR_T* names[] = {
        L"Connect",
        L"Disconnect",
        L"GetFileList",
        L"UploadToPhone",
        L"DownloadFromPhone",
        L"WriteJSON",
        L"ReadJSON",
        L"WriteCSV",
        L"ReadCSV",
        L"GetInfo"
    };

    if (lMethodNum < 0 || lMethodNum > 9)
        return nullptr;

    if (m_pMemMgr)
    {
        WCHAR_T* result = static_cast<WCHAR_T*>(m_pMemMgr->AllocMemory(20 * sizeof(WCHAR_T)));
        // Копируем имя метода...
        return result;
    }

    return names[lMethodNum];
}

long CMyDriver::GetNParams(const long lMethodNum)
{
    switch (lMethodNum)
    {
        case 0: return 1;  // Connect(Идентификатор)
        case 1: return 0;  // Disconnect()
        case 2: return 1;  // GetFileList(Путь)
        case 3: return 2;  // UploadToPhone(ПутьКФайлу, ПутьНаТелефоне)
        case 4: return 2;  // DownloadFromPhone(ПутьНаТелефоне, ПутьНаПК)
        case 5: return 2;  // WriteJSON(Данные, ПутьНаТелефоне)
        case 6: return 1;  // ReadJSON(ПутьНаТелефоне)
        case 7: return 2;  // WriteCSV(Данные, ПутьНаТелефоне)
        case 8: return 1;  // ReadCSV(ПутьНаТелефоне)
        case 9: return 0;  // GetInfo()
        default: return 0;
    }
}

bool CMyDriver::GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant* pvarParamDefValue)
{
    pvarParamDefValue->vt = VTYPE_EMPTY;
    return true;
}

bool CMyDriver::HasRetVal(const long lMethodNum)
{
    switch (lMethodNum)
    {
        case 0: case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: case 9:
            return true;
        default:
            return false;
    }
}

bool CMyDriver::CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray)
{
    try
    {
        switch (lMethodNum)
        {
            // Методы без возвращаемого значения (процедуры)
            default:
                break;
        }
        return true;
    }
    catch (...)
    {
        // Сообщение об ошибке в 1С
        if (m_pBackConn)
        {
            EXCEPINFO info;
            ZeroMemory(&info, sizeof(EXCEPINFO));
            info.wCode = ADDIN_E_FAIL;
            info.bstrSource = SysAllocString(L"MyDriver");
            info.bstrDescription = SysAllocString(L"Ошибка выполнения метода");
            m_pBackConn->AddError(info.wCode, info.bstrSource, info.bstrDescription, info.scode);
        }
        return false;
    }
}

bool CMyDriver::CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
    try
    {
        switch (lMethodNum)
        {
            case 0: // Connect(Идентификатор)
            {
                std::wstring deviceId;
                if (paParams && lSizeArray > 0 && paParams[0].vt != VTYPE_EMPTY)
                {
                    deviceId = paParams[0].pwstrVal;
                }

                // Логика подключения через ADB...
                bool result = ConnectToDevice(deviceId.c_str());

                pvarRetValue->vt = VTYPE_BOOL;
                pvarRetValue->bVal = result ? VARIANT_TRUE : VARIANT_FALSE;
                return true;
            }

            case 3: // UploadToPhone(ПутьКФайлу, ПутьНаТелефоне)
            {
                if (paParams && lSizeArray >= 2)
                {
                    std::wstring srcPath = paParams[0].pwstrVal;
                    std::wstring dstPath = paParams[1].pwstrVal;

                    // Логика adb push...
                    bool result = ADBPush(srcPath.c_str(), dstPath.c_str());

                    pvarRetValue->vt = VTYPE_BOOL;
                    pvarRetValue->bVal = result ? VARIANT_TRUE : VARIANT_FALSE;
                    return true;
                }
                pvarRetValue->vt = VTYPE_BOOL;
                pvarRetValue->bVal = VARIANT_FALSE;
                return true;
            }

            case 6: // ReadJSON(ПутьНаТелефоне)
            {
                if (paParams && lSizeArray >= 1)
                {
                    std::wstring path = paParams[0].pwstrVal;

                    // Чтение JSON с телефона...
                    // Создать ТаблицуЗначений и вернуть как BLOB...

                    pvarRetValue->vt = VTYPE_EMPTY;
                    return true;
                }
                pvarRetValue->vt = VTYPE_EMPTY;
                return true;
            }

            case 9: // GetInfo()
            {
                // Вернуть таблицу с информацией о драйвере
                pvarRetValue->vt = VTYPE_EMPTY;
                return true;
            }

            default:
                pvarRetValue->vt = VTYPE_EMPTY;
                return true;
        }
    }
    catch (...)
    {
        if (m_pBackConn)
        {
            EXCEPINFO info;
            ZeroMemory(&info, sizeof(EXCEPINFO));
            info.wCode = ADDIN_E_FAIL;
            info.bstrSource = SysAllocString(L"MyDriver");
            info.bstrDescription = SysAllocString(L"Ошибка выполнения метода");
            m_pBackConn->AddError(info.wCode, info.bstrSource, info.bstrDescription, info.scode);
        }
        pvarRetValue->vt = VTYPE_EMPTY;
        return false;
    }
}

// ============================================================
// Регистрация расширения встроенного языка
// ============================================================

bool CMyDriver::RegisterExtensionAs(WCHAR_T** wsExtName)
{
    if (m_pMemMgr)
    {
        *wsExtName = static_cast<WCHAR_T*>(m_pMemMgr->AllocMemory((wcslen(EXTENSION_NAME) + 1) * sizeof(WCHAR_T)));
        wcscpy_s(*wsExtName, wcslen(EXTENSION_NAME) + 1, EXTENSION_NAME);
    }
    else
    {
        *wsExtName = SysAllocString(EXTENSION_NAME);
    }
    return *wsExtName != nullptr;
}
```

### 6.3 CMakeLists.txt для сборки

```cmake
cmake_minimum_required(VERSION 3.25)
project(FileSyncDriver VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Исходные файлы
set(SOURCES
    src/Driver.cpp
    src/ADBManager.cpp
    src/FileManager.cpp
)

# Заголовочные файлы
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Настройки для Windows x86_64
if(WIN32)
    add_library(FileSyncDriver SHARED ${SOURCES})

    # Экспорт символов
    set_target_properties(FileSyncDriver PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    # Флаги компиляции
    target_compile_options(FileSyncDriver PRIVATE
        /utf-8
        /EHsc
    )

    # Линковка
    target_link_libraries(FileSyncDriver
        ole32.lib
        advapi32.lib
    )

    # Установка в папку вывода
    set_target_properties(FileSyncDriver PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
    )
endif()
```

### 6.4 Соответствие типов tVariant и 1С:Предприятия

| tVariant тип | 1С тип | Поле tVariant | Описание |
|--------------|---------|---------------|----------|
| `VTYPE_EMPTY` | Неопределено | — | Параметр не передан |
| `VTYPE_I4` | Число | `lVal` | Целое число (32-bit) |
| `VTYPE_BOOL` | Булево | `bVal` | Истина/Ложь |
| `VTYPE_R8` | Число | `dblVal` | Дробное число (64-bit) |
| `VTYPE_PWSTR` | Строка | `pwstrVal` + `wstrLen` | Unicode-строка |
| `VTYPE_BLOB` | ДвоичныеДанные | `pstrVal` + `strLen` | Двоичные данные |
| `VTYPE_DATE` | Дата | `date` | Дата/время |
| `VTYPE_ARRAY` | Массив | `parray` | Массив VARIANT |

### 6.5 Коды сообщений для AddError

| Код | Константа | Поведение |
|-----|-----------|-----------|
| 1000 | `ADDIN_E_NONE` | Без иконки |
| 1001 | `ADDIN_E_ORDINARY` | Иконка ">" |
| 1002 | `ADDIN_E_ATTENTION` | Иконка "!" |
| 1003 | `ADDIN_E_IMPORTANT` | Иконка "!!" |
| 1004 | `ADDIN_E_VERY_IMPORTANT` | Иконка "!!!" |
| 1005 | `ADDIN_E_INFO` | Иконка "i" |
| 1006 | `ADDIN_E_FAIL` | Иконка "err" |
| 1007 | `ADDIN_E_MSGBOX_ATTENTION` | Диалог с "!" |
| 1008 | `ADDIN_E_MSGBOX_INFO` | Диалог с "i" |
| 1009 | `ADDIN_E_MSGBOX_FAIL` | Диалог с "err" |

### 6.6 Важные правила

1. **Память:** Выделять память ТОЛЬКО через `IMemoryManager::AllocMemory()`. Никогда не использовать `new`, `malloc`, `SysAllocString` для данных, передаваемых в 1С.

2. **Ошибки:** Все исключения должны быть перехвачены и переданы через `IAddInDefBase::AddError()`.

3. **Строки:** Строки, возвращаемые 1С, освобождает 1С. Строки, создаваемые компонентой — выделяются через `AllocMemory()`, освобождает 1С.

4. **Потоки:** Компонента может быть загружена на сервере приложений. Не все методы доступны на сервере (строка статуса, события, диалоги).

---

## 7. Установка и регистрация

### 7.1 Ручная установка

1. Скопировать содержимое папки `ADBFileDriver` в целевую директорию (например, `C:\ADBFileDriver\`).

```cmd
xcopy /E /I ADBFileDriver\*.* C:\ADBFileDriver\
```

2. Проверить наличие файлов:
```cmd
dir C:\ADBFileDriver\
```

Должны быть:
- `ADBFileDriver.dll` — основная DLL
- `adb.exe` — Android Debug Bridge
- `ADBDriver.json` — конфигурационный файл (опционально)

### 7.2 Автоматическая установка

Создать установочный скрипт `install.bat`:

```batch
@echo off
echo Installing ADBFileDriver...

copy ADBFileDriver.dll "%ProgramFiles%\ADBFileDriver\" >nul
copy adb.exe "%ProgramFiles%\ADBFileDriver\" >nul

echo Installation complete.
pause
```

### 7.3 Установка через NSIS

```nsis
!include "MUI2.nsh"

Name "ADBFileDriver"
OutFile "ADBFileDriver_Setup.exe"

InstallDir "$PROGRAMFILES\ADBFileDriver"

Section "Install"
    SetOutPath "$INSTDIR"
    File "ADBFileDriver.dll"
    File "adb.exe"
    
    ; Создание ярлыка
    CreateShortcut "$DESKTOP\ADBFileDriver.lnk" "$INSTDIR\ADBFileDriver.dll"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\ADBFileDriver.dll"
    Delete "$INSTDIR\adb.exe"
    RMDir /r "$INSTDIR"
SectionEnd
```

---

## 8. Техническая документация Native API

### 8.1 Интерфейсы компоненты

#### IComponentBase

Базовый интерфейс, от которого наследуется каждая компонента.

| Метод | Описание |
|-------|----------|
| `Init(void* Interface)` | Инициализация, передаёт интерфейс 1С |
| `Done()` | Завершение работы |
| `GetInfo()` | Возвращает версию компоненты |
| `setMemManager(void* memManager)` | Устанавливает менеджер памяти |

#### ILanguageExtender

Расширение встроенного языка 1С.

| Метод | Описание |
|-------|----------|
| `RegisterExtensionAs()` | Имя расширения |
| `GetNProps()` | Количество свойств |
| `FindProp()` | Поиск свойства |
| `GetPropVal()` | Получение значения свойства |
| `SetPropVal()` | Установка значения свойства |
| `GetNMethods()` | Количество методов |
| `FindMethod()` | Поиск метода |
| `GetMethodName()` | Имя метода |
| `GetNParams()` | Количество параметров |
| `CallAsProc()` | Вызов как процедура |
| `CallAsFunc()` | Вызов как функция |

### 8.2 Экспортируемые функции DLL

| Функция | Описание |
|---------|----------|
| `GetClassNames()` | Возвращает список имён объектов (разделитель `\0`) |
| `GetClassObject()` | Создаёт экземпляр объекта |
| `DestroyObject()` | Удаляет экземпляр |
| `SetPlatformCapabilities()` | Устанавливает версию возможностей |
| `GetAttachType()` | Возвращает тип подключения |

### 8.3 Структуры данных

#### tVariant

```cpp
typedef struct tVariant {
    VARTYPE vt;           // Тип значения
    WORD wReserved1;      // Зарезервировано
    WORD wReserved2;      // Зарезервировано
    WORD wReserved3;      // Зарезервировано
    union {
        LONG lVal;        // VTYPE_I4
        VARIANT_BOOL bVal;// VTYPE_BOOL
        DOUBLE dblVal;    // VTYPE_R8
        DATE date;        // VTYPE_DATE
        WCHAR_T* pwstrVal;// VTYPE_PWSTR
        BYTE* pstrVal;    // VTYPE_BLOB
        void* parray;     // VTYPE_ARRAY
        void* pvUser;     // Пользовательский
        int lVal64;       // VTYPE_I8
        struct tm tmVal;  // VTYPE_TM
    };
    DWORD dwActualType;   // Фактический тип
    UINT uUser;           // Пользовательские данные
    char* szUser;         // Пользовательская строка
    GUID pUserGuid;       // GUID
} tVariant;
```

#### EXCEPINFO

```cpp
typedef struct EXCEPINFO {
    WORD wCode;           // Код сообщения
    WORD wCodeSource;     // Источник
    BSTR bstrDescription; // Описание
    BSTR bstrSource;      // Строка источника
    DWORD scode;          // Код ошибки
    DWORD dwHelpContext;  // Контекст справки
    BSTR bstrHelpFile;    // Файл справки
} EXCEPINFO;
```

### 8.4 Примеры ошибок и их обработка

```cpp
// Пример корректной обработки ошибок
bool CMyDriver::CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, 
                           tVariant* paParams, const long lSizeArray)
{
    try
    {
        // Основная логика
        return ProcessMethod(lMethodNum, pvarRetValue, paParams, lSizeArray);
    }
    catch (const std::exception& e)
    {
        // Передача ошибки в 1С
        if (m_pBackConn)
        {
            EXCEPINFO info;
            ZeroMemory(&info, sizeof(EXCEPINFO));
            info.wCode = ADDIN_E_FAIL;
            info.bstrSource = SysAllocString(L"MyDriver");
            
            std::string msg = std::string("Ошибка: ") + e.what();
            info.bstrDescription = SysAllocStringA(msg.c_str());
            info.scode = E_FAIL;
            
            m_pBackConn->AddError(info.wCode, info.bstrSource, 
                                  info.bstrDescription, info.scode);
        }
        pvarRetValue->vt = VTYPE_EMPTY;
        return false;
    }
    catch (...)
    {
        if (m_pBackConn)
        {
            EXCEPINFO info;
            ZeroMemory(&info, sizeof(EXCEPINFO));
            info.wCode = ADDIN_E_FAIL;
            info.bstrSource = SysAllocString(L"MyDriver");
            info.bstrDescription = SysAllocString(L"Неизвестная ошибка");
            info.scode = E_FAIL;
            m_pBackConn->AddError(info.wCode, info.bstrSource, 
                                  info.bstrDescription, info.scode);
        }
        pvarRetValue->vt = VTYPE_EMPTY;
        return false;
    }
}
```

---

## Приложение A. ADB-команды

### A.1 Основные команды

| Команда | Описание |
|---------|----------|
| `adb devices` | Список подключённых устройств |
| `adb push <local> <remote>` | Загрузка файла на устройство |
| `adb pull <remote> <local>` | Скачивание файла с устройства |
| `adb shell ls <path>` | Список файлов |
| `adb shell mkdir <path>` | Создание папки |
| `adb shell rm <path>` | Удаление файла |
| `adb shell stat <path>` | Информация о файле |
| `adb shell df /sdcard` | Свободное место |

### A.2 Примеры использования в коде

```cpp
// Выполнение ADB-команды
std::string ADBCommand(const std::string& cmd)
{
    std::string result;
    char buffer[256];
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

// adb push
bool ADBPush(const std::wstring& local, const std::wstring& remote)
{
    std::string cmd = "adb push \"" + WideToAnsi(local) + "\" \"" + WideToAnsi(remote) + "\"";
    std::string result = ADBCommand(cmd);
    return result.find("1 file pushed") != std::string::npos;
}

// adb pull
bool ADBPull(const std::wstring& remote, const std::wstring& local)
{
    std::string cmd = "adb pull \"" + WideToAnsi(remote) + "\" \"" + WideToAnsi(local) + "\"";
    std::string result = ADBCommand(cmd);
    return result.find("1 file pulled") != std::string::npos;
}
```

---

## Приложение B. Сравнение Native API и COM

| Характеристика | Native API | COM |
|----------------|------------|-----|
| Платформы | Windows, Linux, macOS, ARM, Эльбрус | Только Windows |
| Регистрация | Не требуется | Требуется в реестре |
| Производительность | Выше (прямой вызов) | Ниже (через COM) |
| Сложность | Выше | Ниже |
| Мобильные платформы | Да | Нет |
| Веб-клиент | Нет | Да (через адаптеры) |
| Размер DLL | Больше | Меньше |

---

## Приложение C. Версии документа

| Версия | Дата | Автор | Описание |
|--------|------|-------|----------|
| 1.0 | 28.06.2026 | — | Первоначальная версия |
| 1.1 | 28.06.2026 | Cline | Анализ проекта "мастер пакет" и предложения по решению задачи |

---

## Приложение D. Анализ проекта "мастер пакет" и предложения по решению задачи

### D.1 Текущее состояние проекта

#### 1.1 Архитектура проекта

Проект представляет собой Native API внешнюю компоненту для 1С:Предприятие 8.3. Текущая реализация находится в файлах:
- `innNative.cpp` / `innNative.h` — основная реализация компоненты
- `CMakeLists.txt` — скрипт сборки
- `include/` — заголовочные файлы интерфейсов 1С

#### 1.2 Текущая компонента `innNative`

**Имя расширения встроенного языка:** `InnovaIT`

**Имя объекта компоненты:** `innNative`

**Свойства (1):**
| № | Имя (EN) | Имя (RU) | Чтение | Запись | Описание |
|---|----------|----------|--------|--------|----------|
| 0 | Verison | Версия | Да | Нет | Номер версии компоненты |

**Методы (10):**
| № | Имя (EN) | Имя (RU) | Параметры | Возврат | Описание |
|---|----------|----------|-----------|---------|----------|
| 0 | GetVersion | ПолучитьНомерВерсии | 0 | Текст | Возвращает версию "1.0.0.0" |
| 1 | GetDescription | ПолучитьОписание | 7 | bool | Возвращает описание драйвера (заглушка) |
| 2 | GetLastError | ПолучитьОшибку | 1 | bool | Возвращает последнюю ошибку (заглушка) |
| 3 | GetParameters | ПолучитьПараметры | 1 | XML | Возвращает XML-параметры (заглушка) |
| 4 | SetParameter | УстановитьПараметр | 2 | bool | Устанавливает параметр (заглушка) |
| 5 | Open | Подключить | 1 | bool | Подключение (заглушка с потоком) |
| 6 | Close | Отключить | 1 | bool | Отключение (заглушка с потоком) |
| 7 | DeviceTest | ТестУстройства | 2 | bool | Тест устройства (заглушка) |
| 8 | GetAdditionalActions | ПолучитьДополнительныеДействия | 1 | XML | Дополнительные действия (заглушка) |
| 9 | DoAdditionalAction | ВыполнитьДополнительноеДействие | 1 | bool | Выполнение действия (заглушка) |

#### 1.3 Анализ текущего кода

**Сильные стороны:**
1. Корректная реализация базовых интерфейсов IComponentBase и ILanguageExtender
2. Поддержка двуязычного интерфейса (EN/RU)
3. Кроссплатформенность (Windows/Linux)
4. Использование менеджера памяти 1С (IMemoryManager)
5. Потокобезопасная отправка событий в 1С

**Слабые стороны / Проблемы:**
1. **Отсутствует реальная ADB-функциональность** — все методы являются заглушками
2. **Метод `Open` не работает с ADB** — создаёт только поток-тик, не подключаясь к устройствам
3. **Нет подключения к Android** — нет вызовов adb.exe, нет управления USB-подключениями
4. **Нет обмена файлами** — нет методов push/pull/список файлов
5. **Нет работы с JSON/CSV** — нет сериализации/десериализации данных
6. **Опечатка в имени свойства** — "Verison" вместо "Version"
7. **Поток в `openThreadWindows` работает бесконечно** — нет механизма остановки
8. **Нет обработки ошибок** — исключения не перехватываются

---

### D.2 Анализ документации

#### D.2.1 Что описано в документации (Драйвер_ADB_для_1С.md)

Документация описывает **целевое состояние** компонента `FileSyncDriver` (ProgID: `AddIn.ADBFileDriver`), который должен:

1. **Управление устройствами:**
   - `ПодключитьУстройство()` — подключение к Android-устройству
   - `ОтключитьУстройство()` — отключение
   - `ИдентификаторУстройства` — свойство ID устройства
   - `СтатусПодключения` — статус подключения
   - `СписокПодключенныхУстройств` — таблица устройств

2. **Выгрузка данных НА телефон:**
   - `ЗагрузитьНаТелефон(ПутьКФайлу, ПутьНаТелефоне)` — adb push
   - `ВыгрузитьJSONНаТелефон(ИмяФайла, Данные, ПутьНаТелефоне)` — ТаблицаЗначений → JSON → adb push
   - `ВыгрузитьCSVНаТелефон(ИмяФайла, Данные, ПутьНаТелефоне)` — ТаблицаЗначений → CSV → adb push
   - `СоздатьПапкуНаТелефоне(Путь)` — adb shell mkdir

3. **Загрузка данных С телефона:**
   - `СкачатьСТелефона(ПутьНаТелефоне, ПутьНаПК)` — adb pull
   - `ЗагрузитьJSONСТелефона(ИмяФайла, ПутьНаТелефоне)` — adb pull + JSON → ТаблицаЗначений
   - `ЗагрузитьCSVСТелефона(ИмяФайла, ПутьНаТелефоне)` — adb pull + CSV → ТаблицаЗначений
   - `СписокФайловНаТелефоне(Путь)` — adb shell ls

4. **Прямая работа с данными:**
   - `ЗаписатьJSON(Данные, ПутьНаТелефоне)` — ТаблицаЗначений → JSON на устройстве
   - `ПрочитатьJSONСТелефона(ПутьНаТелефоне)` — JSON на устройстве → ТаблицаЗначений
   - `ЗаписатьCSV(Данные, ПутьНаТелефоне)` — ТаблицаЗначений → CSV на устройстве
   - `ПрочитатьCSVСТелефона(ПутьНаТелефоне)` — CSV на устройстве → ТаблицаЗначений

#### D.2.2 Технология разработки (из документации)

Документация предоставляет:
1. Полный шаблон компоненты с описанием всех методов ILanguageExtender
2. CMakeLists.txt для сборки
3. Соответствие типов tVariant и 1С:Предприятия
4. Коды сообщений для AddError
5. Важные правила (память, ошибки, потоки)
6. Справочник ADB-команд
7. Примеры кода для ADB push/pull

---

### D.3 Разрыв между текущим состоянием и целевым

| Критерий | Текущее состояние | Целевое состояние | Разрыв |
|----------|------------------|-------------------|--------|
| Имя компоненты | `innNative` / `InnovaIT` | `ADBFileDriver` / `FileSyncDriver` | Требуется переименование/создание новой |
| ADB подключение | Нет | Да | **Критический разрыв** |
| adb push/pull | Нет | Да | **Критический разрыв** |
| JSON/CSV | Нет | Да | **Критический разрыв** |
| Список устройств | Нет | Да | **Критический разрыв** |
| Управление папками | Нет | Да | **Критический разрыв** |
| Обработка ошибок | Нет | Да | **Средний разрыв** |
| XML-параметры | Заглушка | Реальные параметры | **Средний разрыв** |

---

### D.4 Предложения по решению задачи

#### Предложение 1: Создание новой компоненты `ADBFileDriver`

**Рекомендация:** Не модифицировать `innNative`, а создать новую компоненту `ADBFileDriver` на основе шаблона из документации.

**Обоснование:**
- `innNative` — это базовый шаблон/мастер-компонента
- Сохранение оригинала позволяет использовать его как референс
- Новая компонента будет иметь чистую архитектуру без лишнего кода

**Шаги реализации:**

##### Шаг 1: Структура новой компоненты

```
ADBFileDriver/
├── ADBFileDriver.h          — заголовочный файл
├── ADBFileDriver.cpp        — основная реализация
├── ADBManager.h             — менеджер ADB-устройств
├── ADBManager.cpp           — реализация ADB
├── FileHelper.h             — помощник файловых операций
├── FileHelper.cpp           — реализация
├── JSONHelper.h             — помощник JSON
├── JSONHelper.cpp           — сериализация/desериализация
├── CSVHelper.h              — помощник CSV
├── CSVHelper.cpp            — сериализация/desериализация
└── CMakeLists_ADB.txt       — скрипт сборки
```

##### Шаг 2: Реализация ADBManager (критический модуль)

```cpp
// ADBManager.h
#pragma once
#include <string>
#include <vector>

struct ADBDevice {
    std::wstring serial;
    std::wstring status;    // "device", "offline", "unauthorized"
    std::wstring model;
};

class ADBManager {
public:
    ADBManager();
    ~ADBManager();

    // Поиск adb.exe в директории DLL и системных путях
    bool FindADB();

    // Получение списка устройств
    std::vector<ADBDevice> GetDevices();

    // Подключение к устройству (проверка доступности)
    bool ConnectToDevice(const std::wstring& serial);

    // Отключение
    void Disconnect();

    // Статус подключения
    std::wstring GetStatus();

private:
    std::wstring ExecuteADBCommand(const std::wstring& cmd);
    std::wstring adbPath_;
    std::wstring connectedDevice_;
    bool initialized_;
};
```

##### Шаг 3: Реализация FileHelper (push/pull/ls/mkdir)

```cpp
// FileHelper.h
#pragma once
#include <string>

class FileHelper {
public:
    // adb push
    static bool PushFile(const std::wstring& localPath, const std::wstring& remotePath, const std::wstring& adbPath);

    // adb pull
    static bool PullFile(const std::wstring& remotePath, const std::wstring& localPath, const std::wstring& adbPath);

    // adb shell ls
    static std::wstring ListFiles(const std::wstring& remotePath, const std::wstring& adbPath);

    // adb shell mkdir
    static bool CreateDirectory(const std::wstring& remotePath, const std::wstring& adbPath);

    // adb shell rm
    static bool DeleteFile(const std::wstring& remotePath, const std::wstring& adbPath);

    // Проверка существования файла
    static bool FileExists(const std::wstring& path, const std::wstring& adbPath);
};
```

##### Шаг 4: Реализация JSONHelper (ТаблицаЗначений ↔ JSON)

```cpp
// JSONHelper.h
#pragma once
#include <string>

// Структура для представления данных 1С
struct ColumnInfo {
    std::wstring name;
    int type;       // 0=Unknown, 1=Число, 2=Строка, 3=Дата, 4=Булево
    int precision;  // для Число
    int scale;      // для Число
    int length;     // для Строка
};

class JSONHelper {
public:
    // ТаблицаЗначений → JSON-строка
    static bool ToJSON(const void* data, std::wstring& result);

    // JSON-строка → ТаблицаЗначений
    // Возвращает указатель на созданную таблицу через менеджер памяти
    static bool FromJSON(const std::wstring& json, void** result);

private:
    // Внутренние парсеры
    static bool ParseJSONString(const std::wstring& json, std::wstring& error);
    static std::wstring EscapeJSONString(const std::wstring& str);
};
```

##### Шаг 5: Основная компонента ADBFileDriver

```cpp
// ADBFileDriver.h
#pragma once
#include "ComponentBase.h"
#include "AddInDefBase.h"
#include "IMemoryManager.h"
#include "ADBManager.h"
#include "FileHelper.h"
#include "JSONHelper.h"
#include "CSVHelper.h"

#define EXTENSION_NAME L"ADBFileDriver"

class CADBFileDriver : public IComponentBase {
private:
    IAddInDefBase* m_pBackConn;
    IMemoryManager* m_pMemMgr;
    bool m_bInitialized;
    ADBManager adbManager_;

    // Вспомогательные методы
    void setWStringToTVariant(tVariant* dest, const wchar_t* source);
    void addError(uint32_t wcode, const wchar_t* source, const wchar_t* descriptor, long code);

public:
    CADBFileDriver();
    virtual ~CADBFileDriver();

    // IComponentBase
    virtual bool Init(void* Interface) override;
    virtual void Done() override;
    virtual long GetInfo() override;
    virtual bool setMemManager(void* memManager) override;

    // Свойства
    virtual long GetNProps() override;
    virtual long FindProp(const WCHAR_T* wsPropName) override;
    virtual const WCHAR_T* GetPropName(long lPropNum, long lPropAlias) override;
    virtual bool GetPropVal(const long lPropNum, tVariant* pvarPropVal) override;
    virtual bool SetPropVal(const long lPropNum, tVariant* pvarPropVal) override;
    virtual bool IsPropReadable(const long lPropNum) override;
    virtual bool IsPropWritable(const long lPropNum) override;

    // Методы
    virtual long GetNMethods() override;
    virtual long FindMethod(const WCHAR_T* wsMethodName) override;
    virtual const WCHAR_T* GetMethodName(const long lMethodNum, const long lMethodAlias) override;
    virtual long GetNParams(const long lMethodNum) override;
    virtual bool GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant* pvarParamDefValue) override;
    virtual bool HasRetVal(const long lMethodNum) override;
    virtual bool CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray) override;
    virtual bool CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray) override;

    // Регистрация
    virtual bool RegisterExtensionAs(WCHAR_T** wsExtName) override;

    // Реализация свойств
    bool GetProp_DeviceID(tVariant* pvarPropVal);
    bool GetProp_Status(tVariant* pvarPropVal);
    bool GetProp_DeviceList(tVariant* pvarPropVal);
    bool SetProp_DeviceID(tVariant* pvarPropVal);

    // Реализация методов
    bool Method_Connect(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_Disconnect(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_PushFile(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_PullFile(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_UploadJSON(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_DownloadJSON(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_UploadCSV(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_DownloadCSV(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_CreateDir(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_ListFiles(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
};
```

##### Шаг 6: Экспортируемые функции

```cpp
// ADBFileDriver.cpp (фрагмент)

extern "C" {

WCHAR_T* GetClassNames()
{
    static WCHAR_T names[] = L"ADBFileDriver";
    return names;
}

long GetClassObject(const WCHAR_T* clsName, IComponentBase** pIntf)
{
    if (_wcsicmp(clsName, L"ADBFileDriver") == 0) {
        *pIntf = new CADBFileDriver();
        return *pIntf ? 1 : 0;
    }
    return 0;
}

long DestroyObject(IComponentBase** pIntf)
{
    if (pIntf && *pIntf) {
        delete *pIntf;
        *pIntf = nullptr;
        return 0;
    }
    return -1;
}

AppCapabilities SetPlatformCapabilities(const AppCapabilities capabilities)
{
    return eAppCapabilitiesLast;
}

AttachType GetAttachType()
{
    return eCanAttachAny;
}

} // extern "C"
```

##### Шаг 7: CMakeLists.txt для ADBFileDriver

```cmake
cmake_minimum_required(VERSION 3.25)
project(ADBFileDriver VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    ADBFileDriver.cpp
    ADBManager.cpp
    FileHelper.cpp
    JSONHelper.cpp
    CSVHelper.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
    add_library(ADBFileDriver SHARED ${SOURCES})

    set_target_properties(ADBFileDriver PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    target_compile_options(ADBFileDriver PRIVATE
        /utf-8
        /EHsc
    )

    target_link_libraries(ADBFileDriver
        ole32.lib
        advapi32.lib
    )

    set_target_properties(ADBFileDriver PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    )
endif()
```

---

#### Предложение 2: Модификация существующего `innNative` (альтернативный вариант)

**Рекомендация:** Добавить ADB-функциональность в существующую компоненту как опциональный модуль.

**Плюсы:**
- Меньше изменений в структуре проекта
- Обратная совместимость

**Минусы:**
- Смешивание ответственности
- Риск нарушения существующего кода

**Не рекомендуется** если есть возможность создать новую компоненту.

---

#### Предложение 3: Архитектурные рекомендации

1. **Разделение ответственности:**
   - `ADBManager` — только работа с ADB (устройства, команды)
   - `FileHelper` — только файловые операции (push/pull)
   - `JSONHelper/CSVHelper` — только сериализация данных
   - `CADBFileDriver` — только интерфейс с 1С

2. **Поиск adb.exe:**
   ```cpp
   bool ADBManager::FindADB() {
       // 1. Проверить текущую директорию DLL
       // 2. Проверить системные переменные PATH
       // 3. Проверить Program Files
       // 4. Проверить реестр (путь установки Android SDK)
   }
   ```

3. **Безопасная работа с памятью:**
   - ВСЕДВА выделять память через `m_pMemMgr->AllocMemory()`
   - ВСЕГДА освобождать через `m_pMemMgr->FreeMemory()`
   - НИКОГДА не использовать `new`/`delete` для данных, передаваемых в 1С

4. **Обработка ошибок:**
   - Все ADB-команды оборачивать в try-catch
   - Ошибки передавать через `m_pBackConn->AddError()`
   - Возвращать `false` при любой ошибке

5. **Конвертация типов:**
   - Использовать существующие `convToShortWchar` / `convFromShortWchar`
   - Поддерживать UTF-16 для Unicode-путей

---

### D.5 План реализации

| Этап | Задача | Оценка | Приоритет |
|------|--------|--------|-----------|
| 1 | Создать ADBManager.h/cpp | 2 часа | КРИТИЧНЫЙ |
| 2 | Реализовать FindADB + GetDevices | 3 часа | КРИТИЧНЫЙ |
| 3 | Реализовать PushFile/PullFile | 3 часа | КРИТИЧНЫЙ |
| 4 | Создать JSONHelper.h/cpp | 4 часа | ВАЖНЫЙ |
| 5 | Создать CSVHelper.h/cpp | 3 часа | ВАЖНЫЙ |
| 6 | Создать ADBFileDriver.h/cpp | 4 часа | КРИТИЧНЫЙ |
| 7 | Реализовать свойства (DeviceID, Status) | 2 часа | КРИТИЧНЫЙ |
| 8 | Реализовать методы (Connect, Disconnect) | 3 часа | КРИТИЧНЫЙ |
| 9 | Реализовать методы файлов (Push, Pull) | 3 часа | ВАЖНЫЙ |
| 10 | Реализовать методы JSON/CSV | 4 часа | ВАЖНЫЙ |
| 11 | CMakeLists.txt и сборка | 2 часа | ВАЖНЫЙ |
| 12 | Тестирование | 4 часа | КРИТИЧНЫЙ |
| 13 | Обновление документации | 2 часа | НОРМАЛЬНЫЙ |
| **Итого** | | **35 часов** | |

---

### D.6 Вопросы для уточнения

1. **Где будет располагаться adb.exe?**
   - В той же директории, что и DLL?
   - В Program Files?
   - В пользовательской директории?

2. **Какова целевая папка на Android-устройстве?**
   - Только `/Download/`?
   - Или пользовательский путь?

3. **Нужна ли поддержка нескольких устройств одновременно?**
   - Или только одно подключённое устройство?

4. **Каков формат JSON для ТаблицыЗначений?**
   - Массив объектов (каждая строка — объект с колонками как свойствами)?
   - Или другой формат?

5. **Нужна ли поддержка CSV с разделителями?**
   - Только запятая?
   - Или выбор разделителя (табуляция, точка с запятой)?

6. **Нужна ли обработка папок (падать на ошибки)?**
   - Если файл уже существует — перезаписывать или ошибка?
   - Если папка существует — ошибка или игнорировать?

7. **Требуется ли логирование?**
   - Запись всех ADB-команд в лог-файл?

---

### D.7 Риски и ограничения

| Риск | Вероятность | Влияние | Митигация |
|------|-------------|---------|-----------|
| adb.exe не найден | Высокая | Критическое | Поиск в нескольких путях + настройка |
| USB-драйверы не установлены | Высокая | Критическое | Документация по установке драйверов | |
| Антивирус блокирует adb.exe | Средняя | Высокое | Добавление в исключения |
| Unicode-пути с длинными именами | Средняя | Среднее | Использование Wide-функций Windows |
| Поток ADB блокирует процесс | Низкая | Высокое | Асинхронные вызовы popen |
| Память не освобождается | Низкая | Критическое | Строгое использование IMemoryManager |

---

### D.8 Результаты реализации

#### D.8.1 Созданные файлы

| Файл | Описание | Статус |
|------|----------|--------|
| `ADBFileDriver/ADBFileDriver.h` | Заголовочный файл компоненты | ✅ Создан |
| `ADBFileDriver/ADBFileDriver.cpp` | Основная реализация | ✅ Создан |
| `ADBFileDriver/CMakeLists.txt` | Скрипт сборки CMake | ✅ Создан |

#### D.8.2 Ответы на вопросы

| Вопрос | Ответ |
|--------|-------|
| Где adb.exe? | В той же директории, что и DLL |
| Количество устройств | Только одно подключённое устройство |
| Формат JSON | Голая строка (JSON-массив объектов) |
| CSV поддержка | Не требуется |
| Обработка папок | Не требуется |
| Логирование | Желательно |

#### D.8.3 Реализованные функции

**Свойства:**
- `СтатусПодключения` (Status) — текущий статус подключения
- `СписокУстройств` (DeviceList) — список устройств в JSON-формате

**Методы:**
| № | Имя (EN) | Имя (RU) | Параметры | Возврат | Описание |
|---|----------|----------|-----------|---------|----------|
| 0 | Connect | ПодключитьУстройство | 0 | bool | Подключение к первому устройству со статусом "device" |
| 1 | Disconnect | ОтключитьУстройство | 0 | bool | Отключение от устройства |
| 2 | PushFile | ЗаписатьНаТелефон | 2 | bool | adb push файла на устройство |
| 3 | PullText | ПрочитатьСТелефона | 1 | string | adb pull файла с чтением содержимого |
| 4 | GetDeviceList | ПолучитьСписокУстройств | 0 | string | Список устройств в JSON-формате |

**Поиск adb.exe:**
1. Текущая директория DLL
2. Системная переменная PATH
3. `C:\Program Files\Android\Android SDK\platform-tools\adb.exe`
4. `%HOMEDRIVE%%HOMEPATH%\AppData\Local\Android\Sdk\platform-tools\adb.exe`
5. Реестр `HKLM\SOFTWARE\Android SDK\AndroidSDKPath`

**Логирование:**
- Статус записывается в `m_strStatus`
- Ошибки передаются через `m_pBackConn->AddError()`

---

---

## Приложение E. Журнал изменений сценария

### 28.06.2026 — 20:39
- LICENSE.MD переведён на русскую версию — коммерческий продукт для России
- Результат сборки сохраняется в папку `1C/Release/` (настроено в CMakeLists.txt)

---

*Документ создан на основе "Технология создания внешних компонент.docx" для платформы 1С:Предприятие 8.3.*
*Последнее обновление: 28.06.2026 — реализована базовая компонента ADBFileDriver*
