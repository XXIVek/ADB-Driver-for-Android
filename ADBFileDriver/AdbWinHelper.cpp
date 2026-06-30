// ============================================================
// AdbWinHelper — реализация обертки над AdbWinApi.dll
// ============================================================

#include "AdbWinHelper.h"
#include <cstdio>
#include <algorithm>

// ============================================================
// Конструктор / Деструктор
// ============================================================

AdbWinHelper::AdbWinHelper()
    : m_hLibrary(nullptr), m_connected(false)
    , m_interface(nullptr), m_readEndpoint(nullptr), m_writeEndpoint(nullptr)
{
    // Инициализация указателей на nullptr
    m_fnEnumInterfaces = nullptr;
    m_fnNextInterface = nullptr;
    m_fnGetInterfaceName = nullptr;
    m_fnGetUsbDeviceDescriptor = nullptr;
    m_fnGetUsbConfigurationDescriptor = nullptr;
    m_fnGetUsbInterfaceDescriptor = nullptr;
    m_fnOpenEndpoint = nullptr;
    m_fnGetEndpointInformation = nullptr;
    m_fnGetEndpointInterface = nullptr;
    m_fnGetDefaultBulkReadEndpoint = nullptr;
    m_fnGetDefaultBulkWriteEndpoint = nullptr;
    m_fnGetSerialNumber = nullptr;
    m_fnCloseHandle = nullptr;
    m_fnReadEndpointSync = nullptr;
    m_fnWriteEndpointSync = nullptr;
}

AdbWinHelper::~AdbWinHelper()
{
    Shutdown();
}

// ============================================================
// Инициализация / Завершение
// ============================================================

bool AdbWinHelper::Initialize()
{
    if (m_hLibrary != nullptr)
        return true;

    // Загрузка AdbWinApi.dll
    m_hLibrary = LoadLibraryW(L"AdbWinApi.dll");
    if (m_hLibrary == nullptr) {
        // Пробуем загрузить из текущей директории
        wchar_t exePath[MAX_PATH];
        DWORD pathLen = GetModuleFileNameW(NULL, exePath, MAX_PATH);
        if (pathLen > 0 && pathLen < MAX_PATH) {
            wchar_t* lastSlash = wcsrchr(exePath, L'\\');
            if (lastSlash) {
                wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - exePath), L"AdbWinApi.dll");
                m_hLibrary = LoadLibraryW(exePath);
            }
        }
    }

    if (m_hLibrary == nullptr)
        return false;

    if (!LoadFunctions()) {
        FreeLibrary(m_hLibrary);
        m_hLibrary = nullptr;
        return false;
    }

    return true;
}

void AdbWinHelper::Shutdown()
{
    if (m_connected)
        Disconnect();

    if (m_hLibrary != nullptr) {
        FreeLibrary(m_hLibrary);
        m_hLibrary = nullptr;
    }
}

bool AdbWinHelper::LoadFunctions()
{
    #define LOAD_FUNC(fn, type) \
        m_fn##fn = (type)GetProcAddress(m_hLibrary, #fn); \
        if (!m_fn##fn) { FreeLibrary(m_hLibrary); m_hLibrary = nullptr; return false; }

    LOAD_FUNC(EnumInterfaces, AdbEnumInterfacesFn)
    LOAD_FUNC(NextInterface, AdbNextInterfaceFn)
    LOAD_FUNC(GetInterfaceName, AdbGetInterfaceNameFn)
    LOAD_FUNC(GetUsbDeviceDescriptor, AdbGetUsbDeviceDescriptorFn)
    LOAD_FUNC(GetUsbConfigurationDescriptor, AdbGetUsbConfigurationDescriptorFn)
    LOAD_FUNC(GetUsbInterfaceDescriptor, AdbGetUsbInterfaceDescriptorFn)
    LOAD_FUNC(OpenEndpoint, AdbOpenEndpointFn)
    LOAD_FUNC(GetEndpointInformation, AdbGetEndpointInformationFn)
    LOAD_FUNC(GetEndpointInterface, AdbGetEndpointInterfaceFn)
    LOAD_FUNC(GetDefaultBulkReadEndpoint, AdbGetDefaultBulkReadEndpointFn)
    LOAD_FUNC(GetDefaultBulkWriteEndpoint, AdbGetDefaultBulkWriteEndpointFn)
    LOAD_FUNC(GetSerialNumber, AdbGetSerialNumberFn)
    LOAD_FUNC(CloseHandle, AdbCloseHandleFn)
    LOAD_FUNC(ReadEndpointSync, AdbReadEndpointSyncFn)
    LOAD_FUNC(WriteEndpointSync, AdbWriteEndpointSyncFn)

    #undef LOAD_FUNC

    return true;
}

// ============================================================
// Перечисление устройств
// ============================================================

std::vector<AdbWinHelper::AdbDeviceInfo> AdbWinHelper::EnumDevices()
{
    std::vector<AdbDeviceInfo> devices;

    if (m_hLibrary == nullptr || m_fnEnumInterfaces == nullptr)
        return devices;

    // Сброс枚举рации
    if (FAILED(m_fnEnumInterfaces()))
        return devices;

    void* interfacePtr = nullptr;
    while (m_fnNextInterface(&interfacePtr, &interfacePtr) == S_OK && interfacePtr != nullptr) {
        AdbDeviceInfo info;
        info.vendorId = 0;
        info.productId = 0;
        info.isAdbDevice = false;

        // Получаем серийный номер
        wchar_t serial[256] = {0};
        DWORD serialLen = 256;
        if (m_fnGetSerialNumber(interfacePtr, serial, serialLen) == S_OK && serial[0] != 0) {
            info.serial = serial;
        }

        // Получаем USB-дескриптор
        USB_DEVICE_DESCRIPTOR devDesc;
        if (m_fnGetUsbDeviceDescriptor(interfacePtr, &devDesc) == S_OK) {
            info.vendorId = devDesc.idVendor;
            info.productId = devDesc.idProduct;

            // Проверяем ADB-устройство (vendor id: 0x04E8 Samsung, 0x18D1 Google, 0x04F2 Xiaomi, и др.)
            // ADB использует класс устройства 0xFF (vendor-specific)
            if (devDesc.bDeviceClass == 0xFF || devDesc.bDeviceSubClass == 0xFF) {
                info.isAdbDevice = true;
            }
        }

        devices.push_back(info);
    }

    return devices;
}

// ============================================================
// Подключение к устройству
// ============================================================

bool AdbWinHelper::ConnectToDevice(const std::wstring& serial)
{
    if (m_connected)
        Disconnect();

    if (!FindInterfaceBySerial(serial))
        return false;

    // Получаем bulk read endpoint
    if (m_fnGetDefaultBulkReadEndpoint(m_interface, &m_readEndpoint) != S_OK || m_readEndpoint == nullptr) {
        return false;
    }

    // Получаем bulk write endpoint
    if (m_fnGetDefaultBulkWriteEndpoint(m_interface, &m_writeEndpoint) != S_OK || m_writeEndpoint == nullptr) {
        return false;
    }

    m_connected = true;
    return true;
}

void AdbWinHelper::Disconnect()
{
    if (m_readEndpoint) {
        m_fnCloseHandle(m_readEndpoint);
        m_readEndpoint = nullptr;
    }
    if (m_writeEndpoint) {
        m_fnCloseHandle(m_writeEndpoint);
        m_writeEndpoint = nullptr;
    }
    m_connected = false;
}

bool AdbWinHelper::FindInterfaceBySerial(const std::wstring& serial)
{
    if (m_fnEnumInterfaces() != S_OK)
        return false;

    void* interfacePtr = nullptr;
    while (m_fnNextInterface(&interfacePtr, &interfacePtr) == S_OK && interfacePtr != nullptr) {
        wchar_t ifaceSerial[256] = {0};
        DWORD serialLen = 256;
        if (m_fnGetSerialNumber(interfacePtr, ifaceSerial, serialLen) == S_OK) {
            if (wcscmp(ifaceSerial, serial.c_str()) == 0) {
                m_interface = interfacePtr;
                return true;
            }
        }
    }

    return false;
}

// ============================================================
// Получение информации об устройстве
// ============================================================

std::wstring AdbWinHelper::GetSerialNumber()
{
    if (m_interface == nullptr)
        return L"";

    wchar_t serial[256] = {0};
    DWORD serialLen = 256;
    if (m_fnGetSerialNumber(m_interface, serial, serialLen) == S_OK) {
        return std::wstring(serial);
    }
    return L"";
}

bool AdbWinHelper::GetDeviceDescriptor(USB_DEVICE_DESCRIPTOR& desc)
{
    if (m_interface == nullptr)
        return false;
    return m_fnGetUsbDeviceDescriptor(m_interface, &desc) == S_OK;
}

bool AdbWinHelper::GetConfigurationDescriptor(USB_CONFIGURATION_DESCRIPTOR& desc)
{
    if (m_interface == nullptr)
        return false;
    return m_fnGetUsbConfigurationDescriptor(m_interface, &desc) == S_OK;
}

bool AdbWinHelper::GetInterfaceDescriptor(USB_INTERFACE_DESCRIPTOR& desc)
{
    if (m_interface == nullptr)
        return false;
    return m_fnGetUsbInterfaceDescriptor(m_interface, &desc) == S_OK;
}

// ============================================================
// Чтение/запись данных
// ============================================================

bool AdbWinHelper::ReadEndpoint(void* buffer, size_t bufferSize, size_t* bytesRead)
{
    if (m_readEndpoint == nullptr || !m_connected)
        return false;
    return m_fnReadEndpointSync(m_readEndpoint, buffer, bufferSize, bytesRead) == S_OK;
}

bool AdbWinHelper::WriteEndpoint(const void* buffer, size_t bufferSize, size_t* bytesWritten)
{
    if (m_writeEndpoint == nullptr || !m_connected)
        return false;
    return m_fnWriteEndpointSync(m_writeEndpoint, const_cast<void*>(buffer), bufferSize, bytesWritten) == S_OK;
}