// ============================================================
// USBDeviceDriver — внешняя компонента для 1С:Предприятие 8.3
// Обмен файлами через USB напрямую без adb.exe
// ============================================================

#include "ADBFileDriver.h"
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

// ============================================================
// Вспомогательные функции
// ============================================================

// ============================================================
// CADBFileDriver — Конструктор / Деструктор
// ============================================================

CADBFileDriver::CADBFileDriver()
    : m_pBackConn(nullptr), m_pMemMgr(nullptr), m_bInitialized(false)
    , m_bConnected(false), m_localeRU(true), m_usbInitialized(false)
{
    m_strStatus = L"Не подключено";
    m_connectedDevice.clear();
}

CADBFileDriver::~CADBFileDriver()
{
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

    // Инициализируем USB helper
    if (m_usbHelper.Initialize()) {
        m_usbInitialized = true;
        m_strStatus = L"USB инициализирован";
    } else {
        m_strStatus = L"Ошибка инициализации USB";
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
    
    if (m_usbInitialized) {
        m_usbHelper.Shutdown();
        m_usbInitialized = false;
    }
}

long CADBFileDriver::GetInfo()
{
    // Версия 2.0.0.0 = 200000
    return 200000;
}

bool CADBFileDriver::setMemManager(void* memManager)
{
    if (memManager == nullptr)
        return false;

    m_pMemMgr = static_cast<IMemoryManager*>(memManager);
    return true;
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
        for (size_t i = 0; i < len; i++) {
            result[i] = (WCHAR_T)source[i];
        }
        return result;
    }

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

    tVarInit(dest);

    size_t len = wcslen(source) + 1;
    TV_VT(dest) = VTYPE_PWSTR;

    if (m_pMemMgr && m_pMemMgr->AllocMemory((void**)&dest->pwstrVal, (unsigned long)(len * sizeof(WCHAR_T)))) {
        for (size_t i = 0; i < len; i++) {
            dest->pwstrVal[i] = (WCHAR_T)source[i];
        }
        dest->wstrLen = (UINT)wcslen(source);
    } else {
        TV_VT(dest) = VTYPE_EMPTY;
        dest->pwstrVal = nullptr;
        dest->wstrLen = 0;
    }
}

void CADBFileDriver::addError(uint32_t wcode, const wchar_t* source, const wchar_t* descriptor, long code)
{
    if (m_pBackConn) {
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
    L"USBState"
};

static const wchar_t* g_PropNamesRU[] = {
    L"СтатусПодключения",
    L"СписокУстройств",
    L"СостояниеUSB"
};

long CADBFileDriver::GetNProps()
{
    return 3;
}

long CADBFileDriver::FindProp(const WCHAR_T* wsPropName)
{
    size_t len = 0;
    const WCHAR_T* tmp = wsPropName;
    while (*tmp++) len++;
    len++;

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

    WCHAR_T* result = allocWString(currentName);
    return result;
}

bool CADBFileDriver::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
    if (!pvarPropVal) return false;

    switch (lPropNum) {
        case 0: // СтатусПодключения
            return GetProp_Status(pvarPropVal);
        case 1: // СписокУстройств
            return GetProp_DeviceList(pvarPropVal);
        case 2: // СостояниеUSB
            return GetProp_USBState(pvarPropVal);
        default:
            TV_VT(pvarPropVal) = VTYPE_EMPTY;
            return false;
    }
}

bool CADBFileDriver::SetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
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
    return false;
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
    if (!m_usbInitialized || !m_usbHelper.IsInitialized()) {
        setWStringToTVariant(pvarPropVal, L"[]");
        return true;
    }

    // Получаем список USB устройств
    auto devices = m_usbHelper.EnumAndroidDevices();
    
    // Формируем JSON
    std::string json = "[\n";
    bool first = true;
    
    for (const auto& dev : devices) {
        if (!first) json += ",\n";
        first = false;
        
        // Конвертируем wchar_t в UTF-8
        int wLen = WideCharToMultiByte(CP_UTF8, 0, dev.serial_number.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string serialStr(wLen, '\0');
        WideCharToMultiByte(CP_UTF8, 0, dev.serial_number.c_str(), -1, &serialStr[0], wLen, nullptr, nullptr);
        serialStr.resize(wLen - 1);
        
        // Конвертируем vendor_id в hex
        wchar_t hexBuf[10];
        swprintf(hexBuf, 10, L"0x%X", dev.vendor_id);
        std::string vendorIdUtf8 = "";
        int wLen2 = WideCharToMultiByte(CP_UTF8, 0, hexBuf, -1, nullptr, 0, nullptr, nullptr);
        if (wLen2 > 0) {
            vendorIdUtf8.resize(wLen2 - 1);
            WideCharToMultiByte(CP_UTF8, 0, hexBuf, -1, &vendorIdUtf8[0], wLen2, nullptr, nullptr);
        }
        json += "  {\"Serial\": \"" + serialStr + "\", \"VendorId\": \"" + vendorIdUtf8 + "\"}";
    }
    
    json += "\n]";
    
    int wLen = MultiByteToWideChar(CP_UTF8, 0, json.c_str(), (int)json.size(), nullptr, 0);
    if (wLen > 0) {
        std::wstring wJson((size_t)wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, json.c_str(), (int)json.size(), &wJson[0], wLen);
        setWStringToTVariant(pvarPropVal, wJson.c_str());
    } else {
        TV_VT(pvarPropVal) = VTYPE_EMPTY;
    }
    
    return true;
}

bool CADBFileDriver::GetProp_USBState(tVariant* pvarPropVal)
{
    TV_VT(pvarPropVal) = VTYPE_BOOL;
    TV_BOOL(pvarPropVal) = (m_usbInitialized && m_usbHelper.IsInitialized()) ? VARIANT_TRUE : VARIANT_FALSE;
    return true;
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
    L"ЗаписатьНаУстройство",
    L"ПрочитатьСУстройства",
    L"ПолучитьСписокУстройств"
};

long CADBFileDriver::GetNMethods()
{
    return 5;
}

long CADBFileDriver::FindMethod(const WCHAR_T* wsMethodName)
{
    size_t len = 0;
    const WCHAR_T* tmp = wsMethodName;
    while (*tmp++) len++;
    len++;

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

    WCHAR_T* result = allocWString(currentName);
    return result;
}

long CADBFileDriver::GetNParams(const long lMethodNum)
{
    switch (lMethodNum) {
        case 0: return 0;  // Connect()
        case 1: return 0;  // Disconnect()
        case 2: return 2;  // PushFile(ПутьНаПК, ПутьНаУстройстве)
        case 3: return 1;  // PullText(ПутьНаУстройстве)
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
    return true;
}

// ============================================================
// Реализация методов
// ============================================================

bool CADBFileDriver::Method_Connect(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    (void)paParams;
    (void)lSizeArray;
    
    try {
        if (!m_usbInitialized || !m_usbHelper.IsInitialized()) {
            UpdateStatus(L"Ошибка: USB не инициализирован");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        // Подключаемся к первому найденному устройству
        if (m_usbHelper.ConnectFirstDevice()) {
            m_connectedDevice = m_usbHelper.GetSerialNumber();
            m_bConnected = true;
            UpdateStatus(L"Подключено: " + m_connectedDevice);
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_TRUE;
            return true;
        } else {
            UpdateStatus(L"Нет доступных USB устройств");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

    } catch (...) {
        UpdateStatus(L"Ошибка при подключении");
        addError(ADDIN_E_FAIL, L"USBDeviceDriver", L"Ошибка подключения", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    }
}

bool CADBFileDriver::Method_Disconnect(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    (void)paParams;
    (void)lSizeArray;
    
    try {
        if (!m_bConnected) {
            UpdateStatus(L"Устройство уже отключено");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_TRUE;
            return true;
        }

        m_usbHelper.Disconnect();
        m_bConnected = false;
        m_connectedDevice.clear();
        UpdateStatus(L"Не подключено");

        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_TRUE;
        return true;

    } catch (...) {
        UpdateStatus(L"Ошибка при отключении");
        addError(ADDIN_E_FAIL, L"USBDeviceDriver", L"Ошибка отключения", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    }
}

bool CADBFileDriver::Method_PushFile(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    (void)lSizeArray;
    
    try {
        if (!paParams || lSizeArray < 2) {
            UpdateStatus(L"Ошибка: недостаточно параметров");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        if (!m_bConnected) {
            UpdateStatus(L"Ошибка: устройство не подключено");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        // Получаем путь на ПК
        std::wstring srcPath;
        if (TV_VT(paParams) == VTYPE_PWSTR) {
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

        // Проверяем существование файла
        if (!PathFileExistsW(srcPath.c_str())) {
            std::wstring err = L"Файл не найден: " + srcPath;
            UpdateStatus(err);
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        // Читаем файл
        FILE* f = nullptr;
        if (_wfopen_s(&f, srcPath.c_str(), L"rb") != 0 || f == nullptr) {
            std::wstring err = L"Ошибка чтения файла: " + srcPath;
            UpdateStatus(err);
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fileSize <= 0) {
            fclose(f);
            UpdateStatus(L"Файл пустой");
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
            return true;
        }

        std::vector<char> fileData((size_t)fileSize);
        fread(&fileData[0], 1, (size_t)fileSize, f);
        fclose(f);

        // Записываем на устройство
        size_t bytesWritten = 0;
        bool ok = m_usbHelper.Write(fileData.data(), fileData.size(), &bytesWritten);
        
        if (ok && (size_t)fileSize == bytesWritten) {
            std::wstring statusMsg = L"Файл записан: " + std::to_wstring(bytesWritten) + L" байт";
            UpdateStatus(statusMsg);
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_TRUE;
        } else {
            std::wstring err = L"Ошибка записи: " + std::to_wstring((unsigned long)GetLastError()) + L" (код " + std::to_wstring((unsigned long)GetLastError()) + L")";
            UpdateStatus(err);
            TV_VT(pvarRetValue) = VTYPE_BOOL;
            TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        }

        return true;

    } catch (const std::exception& e) {
        std::string msg = std::string("Ошибка push: ") + e.what();
        int wLen = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), nullptr, 0);
        std::wstring wMsg((size_t)wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), &wMsg[0], wLen);
        UpdateStatus(wMsg);
        addError(ADDIN_E_FAIL, L"USBDeviceDriver", wMsg.c_str(), E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    } catch (...) {
        UpdateStatus(L"Неизвестная ошибка при push");
        addError(ADDIN_E_FAIL, L"USBDeviceDriver", L"Неизвестная ошибка", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_BOOL;
        TV_BOOL(pvarRetValue) = VARIANT_FALSE;
        return true;
    }
}

bool CADBFileDriver::Method_PullText(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    (void)lSizeArray;
    
    try {
        if (!m_bConnected) {
            UpdateStatus(L"Ошибка: устройство не подключено");
            TV_VT(pvarRetValue) = VTYPE_EMPTY;
            return true;
        }

        // Получаем размер для чтения
        const size_t READ_SIZE = 4096;
        std::vector<char> buffer(READ_SIZE);
        size_t bytesRead = 0;
        
        bool ok = m_usbHelper.Read(buffer.data(), READ_SIZE, &bytesRead);
        
        if (ok && bytesRead > 0) {
            // Конвертируем в wchar_t
            int wLen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), (int)bytesRead, nullptr, 0);
            if (wLen > 0) {
                std::wstring content((size_t)wLen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, buffer.data(), (int)bytesRead, &content[0], wLen);
                setWStringToTVariant(pvarRetValue, content.c_str());
                return true;
            }
        }

        TV_VT(pvarRetValue) = VTYPE_EMPTY;
        return true;

    } catch (...) {
        UpdateStatus(L"Неизвестная ошибка при pull");
        addError(ADDIN_E_FAIL, L"USBDeviceDriver", L"Неизвестная ошибка", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_EMPTY;
        return true;
    }
}

bool CADBFileDriver::Method_GetDeviceList(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray)
{
    (void)paParams;
    (void)lSizeArray;
    
    try {
        if (!m_usbInitialized || !m_usbHelper.IsInitialized()) {
            setWStringToTVariant(pvarRetValue, L"[]");
            return true;
        }

        auto devices = m_usbHelper.EnumAndroidDevices();
        
        std::string json = "[\n";
        bool first = true;
        
        for (const auto& dev : devices) {
            if (!first) json += ",\n";
            first = false;
            
            int wLen = WideCharToMultiByte(CP_UTF8, 0, dev.serial_number.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string serialStr(wLen, '\0');
            WideCharToMultiByte(CP_UTF8, 0, dev.serial_number.c_str(), -1, &serialStr[0], wLen, nullptr, nullptr);
            serialStr.resize(wLen - 1);
            
            json += "  {\"Serial\": \"" + serialStr + "\"}";
        }
        
        json += "\n]";
        
        int wLen = MultiByteToWideChar(CP_UTF8, 0, json.c_str(), (int)json.size(), nullptr, 0);
        if (wLen > 0) {
            std::wstring wJson((size_t)wLen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, json.c_str(), (int)json.size(), &wJson[0], wLen);
            setWStringToTVariant(pvarRetValue, wJson.c_str());
        } else {
            TV_VT(pvarRetValue) = VTYPE_EMPTY;
        }
        
        return true;

    } catch (...) {
        UpdateStatus(L"Ошибка получения списка устройств");
        addError(ADDIN_E_FAIL, L"USBDeviceDriver", L"Ошибка получения списка", E_FAIL);
        TV_VT(pvarRetValue) = VTYPE_EMPTY;
        return true;
    }
}

// ============================================================
// ILanguageExtender — Вызов методов
// ============================================================

bool CADBFileDriver::CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray)
{
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
            case 2: // ЗаписатьНаУстройство(ПутьНаПК, ПутьНаУстройстве)
                return Method_PushFile(pvarRetValue, paParams, lSizeArray);
            case 3: // ПрочитатьСУстройства(ПутьНаУстройстве)
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
            info.bstrSource = SysAllocString(L"USBDeviceDriver");
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
        m_localeRU = true;
        return;
    }

    size_t len = 0;
    const WCHAR_T* tmp = loc;
    while (*tmp++) len++;

    wchar_t* localeStr = new wchar_t[len + 1];
    for (size_t i = 0; i < len + 1; i++) {
        localeStr[i] = (wchar_t)loc[i];
    }

    std::wstring wLoc(localeStr);
    m_localeRU = (wLoc.find(L"ru") != std::wstring::npos);

    delete[] localeStr;

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
    static WCHAR_T names[] = { L'A', L'D', L'B', L'F', L'i', L'l', L'e', L'D', L'r', L'i', L'v', L'e', L'r', L'\0', L'\0' };
    return names;
}

long GetClassObject(const WCHAR_T* clsName, IComponentBase** pIntf)
{
    // 1С передает имя класса для создания
    if (_wcsicmp(clsName, L"ADBFileDriver") == 0) {
        if (!*pIntf) {
            *pIntf = new CADBFileDriver();
            return *pIntf ? 1 : 0;
        }
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