// ============================================================
// ADBFileDriver — внешняя компонента для 1С:Предприятие 8.3
// Обмен файлами через USB/ADB
// ============================================================

#include "ADBFileDriver.h"
#include "AdbWinHelper.h"
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

// ============================================================
// Вспомогательные функции
// ============================================================

// Преобразование wstring в UTF-8 string (для _popen)
static std::string WideToUTF8String(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) return std::string();

    std::string str(sizeNeeded, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], sizeNeeded, NULL, NULL);
    str.resize(sizeNeeded - 1);
    return str;
}

// ============================================================
// CADBFileDriver — Конструктор / Деструктор
// ============================================================

CADBFileDriver::CADBFileDriver()
    : m_pBackConn(nullptr), m_pMemMgr(nullptr), m_bInitialized(false)
    , m_bConnected(false), m_localeRU(true), m_bADBServerRunning(false)
{
    // Инициализация статуса
    m_strStatus = L"Не подключено";
    m_connectedDevice.clear();
}

CADBFileDriver::~CADBFileDriver()
{
    // Автоматическое отключение при уничтожении
    if (m_bConnected) {
        m_bConnected = false;
        m_strStatus = L"Не подключено";
    }
}

// ============================================================
// IComponentBase — Инициализация
// ============================================================

bool CADBFileDriver::Init(void* Interface)
{
    if (Interface == nullptr)
        return false;

    m_pBackConn = static_cast<IAddInDefBase*>(Interface);
    m_bInitialized = true;

    // Поиск adb.exe при инициализации
    FindADB();

    // Запускаем adb сервер и проверяем состояние
    if (!m_strADBPath.empty()) {
        // adb start-server — гарантирует что daemon запущен
        ExecuteADBCommand(L"start-server");
        m_bADBServerRunning = CheckADBServer();
    }

    return true;
}

void CADBFileDriver::Done()
{
    m_bConnected = false;
    m_strStatus = L"Не подключено";
    m_connectedDevice.clear();
    m_bInitialized = false;
    m_pBackConn = nullptr;
    m_pMemMgr = nullptr;
    m_bADBServerRunning = false;
}

long CADBFileDriver::GetInfo()
{
    // Версия 1.0.0.4 = 100004
    return 100004;
}

bool CADBFileDriver::setMemManager(void* memManager)
{
    if (memManager == nullptr)
        return false;

    m_pMemMgr = static_cast<IMemoryManager*>(memManager);
    return true;
}

// ============================================================
// Проверка состояния ADB сервера
// ============================================================

bool CADBFileDriver::CheckADBServer()
{
    if (m_strADBPath.empty())
        return false;

    // Выполняем adb version для проверки живости сервера
    std::wstring fullCmd = m_strADBPath + L" version";
    FILE* pipe = _popen(WideToUTF8String(fullCmd).c_str(), "r");
    if (!pipe)
        return false;

    char buffer[1024];
    bool success = false;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (line.find("ADB") != std::string::npos || line.find("adb") != std::string::npos) {
            success = true;
            break;
        }
    }

    _pclose(pipe);
    return success;
}

// ============================================================
// Выделение строки через m_pMemMgr (без утечек памяти)
// ============================================================

WCHAR_T* CADBFileDriver::allocWString(const wchar_t* source)
{
    if (!source) return nullptr;

    size_t len = wcslen(source) + 1;
    WCHAR_T* result = nullptr;

    if (m_pMemMgr && m_pMemMgr->AllocMemory((void**)&result, (unsigned long)(len * sizeof(WCHAR_T)))) {
        // Копируем символы по одному (для кроссплатформенности)
        for (size_t i = 0; i < len; i++) {
            result[i] = (WCHAR_T)source[i];
        }
        return result;
    }

    // Fallback: если m_pMemMgr недоступен — используем SysAllocString
    return (WCHAR_T*)SysAllocString(source);
}

void CADBFileDriver::freeWString(WCHAR_T* str)
{
    if (!str) return;

    if (m_pMemMgr) {
        m_pMemMgr->FreeMemory((void**)&str);
    } else {
        SysFreeString((BSTR)str);
    }
}

// ============================================================
// Вспомогательные методы
// ============================================================

void CADBFileDriver::setWStringToTVariant(tVariant* dest, const wchar_t* source)
{
    if (!dest || !source) {
        if (dest) {
            TV_VT(dest) = VTYPE_EMPTY;
        }
        return;
    }

    // ЗОЛОТОЕ ПРАВИЛО: Сначала инициализируем tVariant через tVarInit
    tVarInit(dest);

    size_t len = wcslen(source) + 1;
    TV_VT(dest) = VTYPE_PWSTR;

    if (m_pMemMgr && m_pMemMgr->AllocMemory((void**)&dest->pwstrVal, (unsigned long)(len * sizeof(WCHAR_T)))) {
        // Копируем символы напрямую (без ConvToShortWchar)
        for (size_t i = 0; i < len; i++) {
            dest->pwstrVal[i] = (WCHAR_T)source[i];
        }
        dest->wstrLen = (UINT)wcslen(source);
    } else {
        // Если не удалось выделить память — устанавливаем EMPTY
        TV_VT(dest) = VTYPE_EMPTY;
        dest->pwstrVal = nullptr;
        dest->wstrLen = 0;
    }
}

void CADBFileDriver::addError(uint32_t wcode, const wchar_t* source, const wchar_t* descriptor, long code)
{
    if (m_pBackConn) {
        // Выделяем память для сообщения об ошибке
        WCHAR_T* errSource = nullptr;
        WCHAR_T* errDesc = nullptr;

        if (m_pMemMgr) {
            size_t srcLen = wcslen(source) + 1;
            size_t descLen = wcslen(descriptor) + 1;

            m_pMemMgr->AllocMemory((void**)&errSource, (unsigned long)(srcLen * sizeof(WCHAR_T)));
            m_pMemMgr->AllocMemory((void**)&errDesc, (unsigned long)(descLen * sizeof(WCHAR_T)));

            if (errSource) {
                for (size_t i = 0; i < srcLen; i++) {
                    errSource[i] = (WCHAR_T)source[i];
                }
            }
            if (errDesc) {
                for (size_t i = 0; i < descLen; i++) {
                    errDesc[i] = (WCHAR_T)descriptor[i];
                }
            }
        } else {
            errSource = (WCHAR_T*)SysAllocString(source);
            errDesc = (WCHAR_T*)SysAllocString(descriptor);
        }

        if (errSource && errDesc) {
            m_pBackConn->AddError(wcode, errSource, errDesc, code);
        }

        if (errSource) {
            if (m_pMemMgr) m_pMemMgr->FreeMemory((void**)&errSource);
            else SysFreeString((BSTR)errSource);
        }
        if (errDesc) {
            if (m_pMemMgr) m_pMemMgr->FreeMemory((void**)&errDesc);
            else SysFreeString((BSTR)errDesc);
        }
    }
}

bool CADBFileDriver::UpdateStatus(const std::wstring& status)
{
    m_strStatus = status;
    return true;
}

long CADBFileDriver::findName(const wchar_t* names[], const wchar_t* name, const uint32_t size) const
{
    for (uint32_t i = 0; i < size; i++) {
        if (wcscmp(names[i], name) == 0)
            return (long)i;
    }
    return -1;
}

// ============================================================
// Поиск adb.exe
// ============================================================

bool CADBFileDriver::FindADB()
{
    // 1. Проверяем текущую директорию DLL
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string dllPath(exePath);
    size_t pos = dllPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        dllPath = dllPath.substr(0, pos);
        std::string testPath = dllPath + "\\adb.exe";
        if (PathFileExistsA(testPath.c_str())) {
            m_strADBPath = std::wstring(testPath.begin(), testPath.end());
            return true;
        }
    }

    // 2. Проверяем системную переменную PATH
    char pathBuf[32768];
    DWORD result = GetEnvironmentVariableA("PATH", pathBuf, 32768);
    if (result > 0 && result < 32768) {
        char* savePtr = nullptr;
        char* token = strtok_s(pathBuf, ";", &savePtr);
        while (token != nullptr) {
            std::string testPath = std::string(token) + "\\adb.exe";
            if (PathFileExistsA(testPath.c_str())) {
                m_strADBPath = std::wstring(testPath.begin(), testPath.end());
                return true;
            }
            token = strtok_s(nullptr, ";", &savePtr);
        }
    }

    // 3. Проверяем стандартные пути Android SDK
    // 3a. Program Files
    const wchar_t* sdkPaths[] = {
        L"C:\\Program Files\\Android\\Android SDK\\platform-tools\\adb.exe",
        L"C:\\Program Files (x86)\\Android\\Android SDK\\platform-tools\\adb.exe",
    };

    for (int i = 0; i < 2; i++) {
        if (PathFileExistsW(sdkPaths[i])) {
            m_strADBPath = sdkPaths[i];
            return true;
        }
    }

    // 3b. Пользовательский каталог
    wchar_t homeDrive[4];
    wchar_t homePath[260];
    if (GetEnvironmentVariableW(L"HOMEDRIVE", homeDrive, 4) > 0) {
        if (GetEnvironmentVariableW(L"HOMEPATH", homePath, 260) > 0) {
            std::wstring userPath = std::wstring(homeDrive) + homePath;
            std::wstring sdkPath = userPath + L"\\AppData\\Local\\Android\\Sdk\\platform-tools\\adb.exe";
            if (PathFileExistsW(sdkPath.c_str())) {
                m_strADBPath = sdkPath;
                return true;
            }
        }
    }

    // 4. Проверяем реестр
    HKEY hKey;
    wchar_t registryPath[MAX_PATH];
    DWORD regSize = MAX_PATH;

    // Ищем Android SDK в реестре
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Android SDK", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        regSize = MAX_PATH;
        if (RegQueryValueExW(hKey, L"AndroidSDKPath", NULL, NULL, (LPBYTE)registryPath, &regSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::wstring adbPath = std::wstring(registryPath) + L"\\platform-tools\\adb.exe";
            if (PathFileExistsW(adbPath.c_str())) {
                m_strADBPath = adbPath;
                return true;
            }
        } else {
            RegCloseKey(hKey);
        }
    }

    // Если не нашли — оставляем пустую строку (будет ошибка при использовании)
    m_strADBPath.clear();
    return false;
}

// ============================================================
// Выполнение ADB-команд
// ============================================================

std::wstring CADBFileDriver::ExecuteADBCommand(const std::wstring& cmd)
{
    std::string result;
    std::wstring fullCmd = m_strADBPath + L" " + cmd;

    // Используем _popen для выполнения команды
    FILE* pipe = _popen(WideToUTF8String(fullCmd).c_str(), "r");
    if (!pipe) {
        UpdateStatus(L"Ошибка выполнения команды: " + cmd);
        return L"";
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        // Удаляем перевод строки
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';
        if (len > 1 && buffer[len - 2] == '\r')
            buffer[len - 2] = '\0';
        result += buffer;
        result += "\n";
    }

    _pclose(pipe);
    return std::wstring(result.begin(), result.end());
}

std::wstring CADBFileDriver::ExecuteADBCommandToDevice(const std::wstring& cmd)
{
    std::wstring command = cmd;

    // Если есть подключённое устройство — добавляем -s <serial>
    if (!m_connectedDevice.empty()) {
        // Вставляем -s <serial> после adb
        size_t spacePos = command.find(L' ');
        if (spacePos != std::wstring::npos) {
            std::wstring before = command.substr(0, spacePos + 1);
            std::wstring after = command.substr(spacePos);
            command = before + L"-s " + m_connectedDevice + after;
        } else {
            command = L"-s " + m_connectedDevice + L" " + command;
        }
    }

    return ExecuteADBCommand(command);
}

// ============================================================
// ILanguageExtender — Регистрация
// ============================================================

bool CADBFileDriver::RegisterExtensionAs(WCHAR_T** wsExtName)
{
    if (m_pMemMgr) {
        size_t len = wcslen(EXTENSION_NAME) + 1;
        m_pMemMgr->AllocMemory((void**)wsExtName, (unsigned long)(len * sizeof(WCHAR_T)));
        if (*wsExtName) {
            for (size_t i = 0; i < len; i++) {
                (*wsExtName)[i] = (WCHAR_T)EXTENSION_NAME[i];
            }
        }
    } else {
        *wsExtName = SysAllocString(EXTENSION_NAME);
    }
    return *wsExtName != nullptr;
}

// ============================================================
// ILanguageExtender — Свойства
// ============================================================

static const wchar_t* g_PropNamesEN[] = {
    L"Status",
    L"DeviceList",
    L"ADBServerState"
};

static const wchar_t* g_PropNamesRU[] = {
    L"СтатусПодключения",
    L"СписокУстройств",
    L"СостояниеADBСервера"
};

long CADBFileDriver::GetNProps()
{
    return 3; // epLast = 3
}

long CADBFileDriver::FindProp(const WCHAR_T* wsPropName)
{
    // Конвертируем WCHAR_T* в wchar_t* для сравнения
    size_t len = 0;
    const WCHAR_T* tmp = wsPropName;
    while (*tmp++) len++;
    len++; // null-terminator

    wchar_t* propName = new wchar_t[len];
    for (size_t i = 0; i < len; i++) {
        propName[i] = (wchar_t)wsPropName[i];
    }

    std::wstring wName(propName);
    delete[] propName;

    for (int i = 0; i < 3; i++) {
        if (wcscmp(g_PropNamesEN[i], wName.c_str()) == 0) return i;
        if (wcscmp(g_PropNamesRU[i], wName.c_str()) == 0) return i;
    }
    return -1;
}

const WCHAR_T* CADBFileDriver::GetPropName(long lPropNum, long lPropAlias)
{
    if (lPropNum < 0 || lPropNum > 2)
        return nullptr;

    const wchar_t* currentName = nullptr;
    if (lPropAlias == 0) {
        currentName = g_PropNamesEN[lPropNum];
    } else {
        currentName = g_PropNamesRU[lPropNum];
    }

    // Выделяем память через m_pMemMgr (без утечек)
    WCHAR_T* result = allocWString(currentName);
    return result;
}

bool CADBFileDriver::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
    if (!pvarPropVal) return false;

    // Обновляем состояние ADB сервера при каждом чтении
    if (!m_strADBPath.empty()) {
        m_bADBServerRunning = CheckADBServer();
    }

    switch (lPropNum) {
        case 0: // СтатусПодключения
            return GetProp_Status(pvarPropVal);
        case 1: // СписокУстройств
            return GetProp_DeviceList(pvarPropVal);
        case 2: // СостояниеADBСервера
            return GetProp_ADBServerState(pvarPropVal);
        default:
            TV_VT(pvarPropVal) = VTYPE_EMPTY;
            return false;
    }
}

bool CADBFileDriver::SetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
    // Свойства только для чтения
    (void)lPropNum;
    (void)pvarPropVal;
    return false;
}

bool CADBFileDriver::IsPropReadable(const long lPropNum)
{
    return lPropNum >= 0 && lPropNum <= 2;
}

bool CADBFileDriver::IsPropWritable(const long lPropNum)
{
    return false; // Все свойства только для чтения
}

// ============================================================
// Реализация свойств
// ============================================================

bool CADBFileDriver::GetProp_Status(tVariant* pvarPropVal)
{
    setWStringToTVariant(pvarPropVal, m_strStatus.c_str());
    return true;
}

bool CADBFileDriver::GetProp_DeviceList(tVariant* pvarPropVal)
{
    // Если ADB сервер не запущен — возвращаем пустой массив
    if (!m_bADBServerRunning) {
        setWStringToTVariant(pvarPropVal, L"[]");
        return true;
    }

    std::string jsonStr = GetDevicesJSON();

    // Конвертируем UTF-8 в wchar_t
    int wLen = MultiByteToWideChar(CP_UTF8, 0, jsonStr.c_str(), (int)jsonStr.size(), nullptr, 0);
    if (wLen <= 0) {
        TV_VT(pvarPropVal) = VTYPE_EMPTY;
        return false;
    }

    std::wstring wJson((size_t)wLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, jsonStr.c_str(), (int)jsonStr.size(), &wJson[0], wLen);

    setWStringToTVariant(pvarPropVal, wJson.c_str());
    return true;
}

bool CADBFileDriver::GetProp_ADBServerState(tVariant* pvarPropVal)
{
    // Обновляем состояние
    if (!m_strADBPath.empty()) {
        m_bADBServerRunning = CheckADBServer();
    }

    TV_VT(pvarPropVal) = VTYPE_BOOL;
    TV_BOOL(pvarPropVal) = m_bADBServerRunning ? VARIANT_TRUE : VARIANT_FALSE;
    return true;
}

// ============================================================
// Получение списка устройств в JSON
// ============================================================

std::string CADBFileDriver::GetDevicesJSON()
{
    // Сначала пробуем adb devices -l (с деталями)
    std::wstring resultW = ExecuteADBCommand(L"devices -l");

    // Конвертируем wstring в UTF-8 string для парсинга
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, resultW.c_str(), (int)resultW.size(), NULL, 0, NULL, NULL);
    std::string result(utf8Size, '\0');
    if (utf8Size > 0) {
        WideCharToMultiByte(CP_UTF8, 0, resultW.c_str(), (int)resultW.size(), &result[0], utf8Size, NULL, NULL);
    }

    // Если devices -l вернул пустой результат — пробуем просто adb devices
    if (result.empty() || result.find("device") == std::string::npos) {
        resultW = ExecuteADBCommand(L"devices");
        utf8Size = WideCharToMultiByte(CP_UTF8, 0, resultW.c_str(), (int)resultW.size(), NULL, 0, NULL, NULL);
        if (utf8Size > 0) {
            std::string result2(utf8Size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, resultW.c_str(), (int)resultW.size(), &result2[0], utf8Size, NULL, NULL);
            if (!result2.empty()) {
                result = result2;
            }
        }
    }

    // Если всё ещё пусто — возвращаем пустой массив
    if (result.empty()) {
        return "[]";
    }

    std::string json = "[\n";
    bool first = true;

    // Парсим вывод
    std::istringstream stream(result);
    std::string line;
    while (std::getline(stream, line)) {
        // Пропускаем заголовок (List of devices attached) и пустые строки
        if (line.find("List of devices") != std::string::npos || line.empty()) {
            continue;
        }

        // Ищем статус устройства
        std::string status = "";
        size_t tabPos = line.find('\t');
        if (tabPos != std::string::npos) {
            status = line.substr(tabPos + 1);
        } else {
            // Формат без табуляции: serial\tstatus
            continue; // Неизвестный формат
        }

        // Проверяем статус
        bool isDevice = (status.find("device") != std::string::npos);
        bool isOffline = (status.find("offline") != std::string::npos);
        bool isUnauthorized = (status.find("unauthorized") != std::string::npos);

        // Включаем все устройства: device, offline, unauthorized
        if (!isDevice && !isOffline && !isUnauthorized) {
            continue;
        }

        // Получаем serial (первая часть до табуляции)
        size_t tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        std::string serial = line.substr(0, tab1);

        // Получаем модель если есть
        std::string model = "";
        if (tab1 + 1 < line.size()) {
            std::string extra = line.substr(tab1 + 1);
            size_t modelPos = extra.find("model:");
            if (modelPos != std::string::npos) {
                model = extra.substr(modelPos + 6);
            }
        }

        // Конвертируем в UTF-8 JSON (escape-им спецсимволы)
        auto escapeJSON = [](const std::string& s) -> std::string {
            std::string out;
            for (char c : s) {
                if (c == '"') out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else if (c == '\t') out += "\\t";
                else out += c;
            }
            return out;
        };

        std::string escSerial = escapeJSON(serial);
        std::string escModel = escapeJSON(model);

        if (!first) json += ",\n";
        first = false;

        json += "  {\"Serial\": \"" + escSerial + "\", \"Status\": \"" + status + "\"";
        if (!escModel.empty()) {
            json += ", \"Model\": \"" + escModel + "\"";
        }
        json += "}";
    }

    json += "\n]";
    return json;
}

// ============================================================
// ILanguageExtender — Методы
// ============================================================

static const wchar_t* g_MethodNamesEN[] = {
    L"Connect",
    L"Disconnect",
    L"PushFile",
    L"PullText",
    L"GetDeviceList"
};

static const wchar_t* g_MethodNamesRU[] = {
    L"ПодключитьУстройство",
    L"ОтключитьУстройство",
    L"ЗаписатьНаТелефон",
    L"ПрочитатьСТелефона",
    L"ПолучитьСписокУстройств"
};

long CADBFileDriver::GetNMethods()
{
    return 5; // emLast = 5
}

long CADBFileDriver::FindMethod(const WCHAR_T* wsMethodName)
{
    // Конвертируем WCHAR_T* в wchar_t* для сравнения
    size_t len = 0;
    const WCHAR_T* tmp = wsMethodName;
    while (*tmp++) len++;
    len++; // null-terminator

    wchar_t* methodName = new wchar_t[len];
    for (size_t i = 0; i < len; i++) {
        methodName[i] = (wchar_t)wsMethodName[i];
    }

    std::wstring wName(methodName);
    delete[] methodName;

    for (int i = 0; i < 5; i++) {
        if (wcscmp(g_MethodNamesEN[i], wName.c_str()) == 0) return i;
        if (wcscmp(g_MethodNamesRU[i], wName.c_str()) == 0) return i;
    }
    return -1;
}

const WCHAR_T* CADBFileDriver::GetMethodName(const long lMethodNum, const long lMethodAlias)
{
    if (lMethodNum < 0 || lMethodNum > 4)
        return nullptr;

    const wchar_t* currentName = (lMethodAlias == 0) ? g_MethodNamesEN[lMethodNum] : g_MethodNamesRU[lMethodNum];

    // Выделяем память через m_pMemMgr (без утечек)
    WCHAR_T* result = allocWString(currentName);
    return result;
}

long CADBFileDriver::GetNParams(const long lMethodNum)
{
    switch (lMethodNum) {
        case 0: return 0;  // Connect()
        case 1: return 0;  // Disconnect()
        case 2: return 2;  // PushFile(ПутьНаПК, ПутьНаТелефоне)
        case 3: return 1;  // PullText(ПутьНаТелефоне)
        case 4: return 0;  // GetDeviceList()
        default: return 0;
    }
}

bool CADBFileDriver::GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant* pvarParamDefValue)
{
    TV_VT(pvarParamDefValue) = VTYPE_EMPTY;
    (void)lMethodNum;
    (void)lParamNum;
    return true;
}

bool CADBFileDriver::HasRetVal(const long lMethodNum)
{
    (void)lMethodNum;
    return true; // Все методы возвращают значение
}

// ============================================================
// Реализация методов
// ============================================================

bool CADBFileDriver::Method_Connect(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    try {
        // Проверяем, найден ли adb.exe
        if (m_strADBPath.empty()) {
            if (!FindADB()) {
                UpdateStatus(L"Ошибка: adb.exe не найден");
                TV_VT(pvarRetValue) = VTYPE_BOOL;
                TV_BOOL(pvarRetValue) = VARIANT_FALSE;
                return true;
            }
        }

        // Получаем список устройств
        std::wstring devicesOutputW = ExecuteADBCommand(L"devices");
        // Конвертируем wstring в UTF-8 string для парсинга
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, devicesOutputW.c_str(), (int)devicesOutputW.size(), NULL, 0, NULL, NULL);
        std::string devicesOutput(utf8Size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, devicesOutputW.c_str(), (int)devicesOutputW.size(), &devicesOutput[0], utf8Size, NULL, NULL);

        if (devicesOutput.empty()) {
            UpdateStatus(L"Ошибка: не удалось получить список устройств");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        // Парсим первое устройство со статусом "device"
        std::istringstream stream(devicesOutput);
        std::string line;
        bool first = true;
        while (std::getline(stream, line)) {
            if (first) { first = false; continue; } // Пропускаем заголовок

            size_t tabPos = line.find('\t');
            if (tabPos == std::string::npos) continue;

            std::string serial = line.substr(0, tabPos);
            std::string status = (tabPos + 1 < line.size()) ? line.substr(tabPos + 1) : "";

            if (status == "device") {
                // Конвертируем серийный номер из ANSI в UTF-16
                int wLen = MultiByteToWideChar(CP_UTF8, 0, serial.c_str(), (int)serial.size(), nullptr, 0);
                if (wLen > 0) {
                    m_connectedDevice.resize((size_t)wLen, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, serial.c_str(), (int)serial.size(), &m_connectedDevice[0], wLen);
                    m_bConnected = true;
                    UpdateStatus(L"Подключено: " + m_connectedDevice);
                    TV_VT(pvarRetValue) = VTYPE_BOOL;
                    TV_BOOL(pvarRetValue) = (VARIANT_BOOL)VARIANT_TRUE;
                    return true;
                }
            }
        }

        // Устройств со статусом "device" не найдено
        UpdateStatus(L"Нет подключенных устройств");
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = (VARIANT_BOOL)VARIANT_FALSE;
        return true;

    } catch (const std::exception& e) {
        std::string msg = std::string("Ошибка подключения: ") + e.what();
        int wLen = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), nullptr, 0);
        std::wstring wMsg((size_t)wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), &wMsg[0], wLen);
        UpdateStatus(wMsg);
        addError(ADDIN_E_FAIL, L"ADBFileDriver", wMsg.c_str(), E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    } catch (...) {
        UpdateStatus(L"Неизвестная ошибка при подключении");
        addError(ADDIN_E_FAIL, L"ADBFileDriver", L"Неизвестная ошибка", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    }
}

bool CADBFileDriver::Method_Disconnect(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    try {
        if (!m_bConnected) {
            UpdateStatus(L"Устройство уже отключено");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = (VARIANT_BOOL)VARIANT_TRUE;
            return true;
        }

        m_bConnected = false;
        m_connectedDevice.clear();
        UpdateStatus(L"Не подключено");

        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = (VARIANT_BOOL)VARIANT_TRUE;
        return true;

    } catch (...) {
        UpdateStatus(L"Ошибка при отключении");
        addError(ADDIN_E_FAIL, L"ADBFileDriver", L"Ошибка отключения", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    }
}

bool CADBFileDriver::Method_PushFile(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    try {
        // Проверяем параметры
        if (!paParams || lSizeArray < 2) {
            UpdateStatus(L"Ошибка: недостаточно параметров");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        // Получаем пути
        std::wstring srcPath, dstPath;

        if (TV_VT(paParams) == VTYPE_PWSTR) {
            // Конвертируем WCHAR_T* в wchar_t* без утечек
            size_t len = 0;
            const WCHAR_T* tmp = TV_WSTR(paParams);
            while (*tmp++) len++;

            wchar_t* tmpPath = new wchar_t[len + 1];
            for (size_t i = 0; i < len + 1; i++) {
                tmpPath[i] = (wchar_t)TV_WSTR(paParams)[i];
            }
            srcPath = tmpPath;
            delete[] tmpPath;
        } else {
            UpdateStatus(L"Ошибка: первый параметр должен быть строкой");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        if (TV_VT(&paParams[1]) == VTYPE_PWSTR) {
            size_t len = 0;
            const WCHAR_T* tmp = TV_WSTR(&paParams[1]);
            while (*tmp++) len++;

            wchar_t* tmpPath = new wchar_t[len + 1];
            for (size_t i = 0; i < len + 1; i++) {
                tmpPath[i] = (wchar_t)TV_WSTR(&paParams[1])[i];
            }
            dstPath = tmpPath;
            delete[] tmpPath;
        } else {
            UpdateStatus(L"Ошибка: второй параметр должен быть строкой");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        // Проверяем существование исходного файла
        if (!PathFileExistsW(srcPath.c_str())) {
            std::wstring err = L"Файл не найден: " + srcPath;
            UpdateStatus(err);
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        // Выполняем adb push
        std::wstring cmd = L"push \"" + srcPath + L"\" \"" + dstPath + L"\"";
        std::wstring resultW = ExecuteADBCommandToDevice(cmd);
        // Конвертируем в lowercase для проверки
        std::wstring resultLowerW = resultW;
        std::transform(resultLowerW.begin(), resultLowerW.end(), resultLowerW.begin(), ::tolower);

        if (resultLowerW.find(L"1 file pushed") != std::wstring::npos ||
            resultLowerW.find(L"success") != std::wstring::npos) {
            UpdateStatus(L"Файл выгружен: " + dstPath);
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = (VARIANT_BOOL)VARIANT_TRUE;
            return true;
        } else {
            std::wstring err = L"Ошибка push: " + resultW;
            UpdateStatus(err);
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

    } catch (const std::exception& e) {
        std::string msg = std::string("Ошибка push: ") + e.what();
        int wLen = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), nullptr, 0);
        std::wstring wMsg((size_t)wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), &wMsg[0], wLen);
        UpdateStatus(wMsg);
        addError(ADDIN_E_FAIL, L"ADBFileDriver", wMsg.c_str(), E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    } catch (...) {
        UpdateStatus(L"Неизвестная ошибка при push");
        addError(ADDIN_E_FAIL, L"ADBFileDriver", L"Неизвестная ошибка", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    }
}

bool CADBFileDriver::Method_PullText(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    try {
        // Проверяем параметры
        if (!paParams || lSizeArray < 1) {
            UpdateStatus(L"Ошибка: недостаточно параметров");
            TV_VT(pvarRetValue) = VTYPE_EMPTY;
            return true;
        }

        // Получаем путь на телефоне
        std::wstring srcPath;
        if (TV_VT(paParams) == VTYPE_PWSTR) {
            // Конвертируем WCHAR_T* в wchar_t* без утечек
            size_t len = 0;
            const WCHAR_T* tmp = TV_WSTR(paParams);
            while (*tmp++) len++;

            wchar_t* tmpPath = new wchar_t[len + 1];
            for (size_t i = 0; i < len + 1; i++) {
                tmpPath[i] = (wchar_t)TV_WSTR(paParams)[i];
            }
            srcPath = tmpPath;
            delete[] tmpPath;
        } else {
            UpdateStatus(L"Ошибка: первый параметр должен быть строкой");
            TV_VT(pvarRetValue) = VTYPE_EMPTY;
            return true;
        }

        // Создаём временный файл для скачивания
        wchar_t tempPath[MAX_PATH];
        DWORD res = GetTempFileNameW(L"C:\\Temp", L"adb_", 0, tempPath);
        if (res == 0) {
            // Пробуем TEMP
            wchar_t tempDir[MAX_PATH];
            DWORD tempLen = GetTempPathW(MAX_PATH, tempDir);
            if (tempLen == 0 || tempLen > MAX_PATH) {
                wcscpy_s(tempDir, L"C:\\Windows\\Temp\\");
            }
            res = GetTempFileNameW(tempDir, L"adb_", 0, tempPath);
        }

        if (res == 0) {
            UpdateStatus(L"Ошибка: не удалось создать временный файл");
            TV_VT(pvarRetValue) = VTYPE_EMPTY;
            return true;
        }

        // Выполняем adb pull
        std::wstring cmd = L"pull \"" + srcPath + L"\" \"" + std::wstring(tempPath) + L"\"";
        std::wstring resultW = ExecuteADBCommandToDevice(cmd);
        // Конвертируем в lowercase для проверки
        std::wstring resultLowerW = resultW;
        std::transform(resultLowerW.begin(), resultLowerW.end(), resultLowerW.begin(), ::tolower);

        if (resultLowerW.find(L"1 file pulled") != std::wstring::npos ||
            resultLowerW.find(L"success") != std::wstring::npos) {
            // Читаем временный файл
            FILE* f = nullptr;
            if (_wfopen_s(&f, tempPath, L"rb") == 0 && f != nullptr) {
                fseek(f, 0, SEEK_END);
                long fileSize = ftell(f);
                fseek(f, 0, SEEK_SET);

                if (fileSize > 0) {
                    std::vector<char> buffer((size_t)fileSize);
                    fread(&buffer[0], 1, (size_t)fileSize, f);
                    fclose(f);

                    // Удаляем временный файл
                    DeleteFileW(tempPath);

                    // Конвертируем в UTF-16
                    int wLen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), (int)fileSize, nullptr, 0);
                    if (wLen > 0) {
                        std::wstring content((size_t)wLen, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, buffer.data(), (int)fileSize, &content[0], wLen);
                        setWStringToTVariant(pvarRetValue, content.c_str());
                        return true;
                    }
                } else {
                    fclose(f);
                    DeleteFileW(tempPath);
                }
            }
            UpdateStatus(L"Файл пустой или ошибка чтения");
            TV_VT(pvarRetValue) = VTYPE_EMPTY;
            return true;
        } else {
            std::wstring err = L"Ошибка pull: " + resultW;
            UpdateStatus(err);
            TV_VT(pvarRetValue) = VTYPE_EMPTY;
            return true;
        }

    } catch (const std::exception& e) {
        std::string msg = std::string("Ошибка pull: ") + e.what();
        int wLen = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), nullptr, 0);
        std::wstring wMsg((size_t)wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), &wMsg[0], wLen);
        UpdateStatus(wMsg);
        addError(ADDIN_E_FAIL, L"ADBFileDriver", wMsg.c_str(), E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_EMPTY;
        return true;
    } catch (...) {
        UpdateStatus(L"Неизвестная ошибка при pull");
        addError(ADDIN_E_FAIL, L"ADBFileDriver", L"Неизвестная ошибка", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_EMPTY;
        return true;
    }
}

bool CADBFileDriver::Method_GetDeviceList(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    (void)paParams;
    (void)lSizeArray;
    try {
        std::string jsonStr = GetDevicesJSON();

        // Конвертируем UTF-8 в wchar_t
        int wLen = MultiByteToWideChar(CP_UTF8, 0, jsonStr.c_str(), (int)jsonStr.size(), nullptr, 0);
        if (wLen > 0) {
            std::wstring wJson((size_t)wLen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, jsonStr.c_str(), (int)jsonStr.size(), &wJson[0], wLen);
            setWStringToTVariant(pvarRetValue, wJson.c_str());
        } else {
            TV_VT(pvarRetValue) = VTYPE_EMPTY;
        }
        return true;

    } catch (...) {
        UpdateStatus(L"Ошибка получения списка устройств");
        addError(ADDIN_E_FAIL, L"ADBFileDriver", L"Ошибка получения списка устройств", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_EMPTY;
        return true;
    }
}

// ============================================================
// ILanguageExtender — Вызов методов
// ============================================================

bool CADBFileDriver::CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray)
{
    // Все методы возвращают значения — вызываем CallAsFunc
    (void)lMethodNum;
    (void)paParams;
    (void)lSizeArray;
    return false;
}

bool CADBFileDriver::CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
    try {
        switch (lMethodNum) {
            case 0: // ПодключитьУстройство()
                return Method_Connect(pvarRetValue, paParams, lSizeArray);
            case 1: // ОтключитьУстройство()
                return Method_Disconnect(pvarRetValue, paParams, lSizeArray);
            case 2: // ЗаписатьНаТелефон(ПутьНаПК, ПутьНаТелефоне)
                return Method_PushFile(pvarRetValue, paParams, lSizeArray);
            case 3: // ПрочитатьСТелефона(ПутьНаТелефоне)
                return Method_PullText(pvarRetValue, paParams, lSizeArray);
            case 4: // ПолучитьСписокУстройств()
                return Method_GetDeviceList(pvarRetValue, paParams, lSizeArray);
            default:
                TV_VT(pvarRetValue) = VTYPE_EMPTY;
                return false;
        }
    } catch (...) {
        if (m_pBackConn) {
            EXCEPINFO info;
            ZeroMemory(&info, sizeof(EXCEPINFO));
            info.wCode = ADDIN_E_FAIL;
            info.bstrSource = SysAllocString(L"ADBFileDriver");
            info.bstrDescription = SysAllocString(L"Ошибка выполнения метода");
            m_pBackConn->AddError(info.wCode, info.bstrSource, info.bstrDescription, info.scode);
            SysFreeString(info.bstrSource);
            SysFreeString(info.bstrDescription);
        }
        TV_VT(pvarRetValue) = VTYPE_EMPTY;
        return false;
    }
}

// ============================================================
// LocaleBase — реализация
// ============================================================

void CADBFileDriver::SetLocale(const WCHAR_T* loc)
{
    if (!loc) {
        m_localeRU = true; // По умолчанию — русский
        return;
    }

    // Конвертируем WCHAR_T* в wchar_t* для сравнения
    size_t len = 0;
    const WCHAR_T* tmp = loc;
    while (*tmp++) len++;

    wchar_t* localeStr = new wchar_t[len + 1];
    for (size_t i = 0; i < len + 1; i++) {
        localeStr[i] = (wchar_t)loc[i];
    }

    // Проверяем на русский язык
    std::wstring wLoc(localeStr);
    m_localeRU = (wLoc.find(L"ru") != std::wstring::npos);

    delete[] localeStr;

    // Обновляем статус
    if (m_localeRU) {
        m_strStatus = L"Не подключено";
    } else {
        m_strStatus = L"Not connected";
    }
}

// ============================================================
// Экспортируемые функции DLL
// ============================================================

extern "C" {

const WCHAR_T* GetClassNames()
{
    /* 1С ожидает список имён классов, разделённых \0, с двойным \0 в конце */
    static WCHAR_T names[] = { L'A', L'D', L'B', L'F', L'i', L'l', L'e', L'D', L'r', L'i', L'v', L'e', L'r', L'e', L'r', L'\0', L'\0' };
    return names;
}

long GetClassObject(const WCHAR_T* clsName, IComponentBase** pIntf)
{
    if (!*pIntf) {
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
    (void)capabilities;
    return eAppCapabilitiesLast;
}

long GetAttachType()
{
    return 0;
}

} // extern "C"