// ============================================================
// AdbWinHelper — обертка над AdbWinApi.dll для работы с ADB
// через USB напрямую без вызова adb.exe
// ============================================================

#pragma once

#include <windows.h>
#include <string>
#include <vector>

// Структуры данных USB
typedef struct _USB_DEVICE_DESCRIPTOR {
    USHORT  uSize;
    USHORT  uBcdUSB;
    UCHAR   bDeviceClass;
    UCHAR   bDeviceSubClass;
    UCHAR   bDeviceProtocol;
    UCHAR   bMaxPacketSize0;
    USHORT  idVendor;
    USHORT  idProduct;
    USHORT  bcdDevice;
    UCHAR   iManufacturer;
    UCHAR   iProduct;
    UCHAR   iSerialNumber;
    UCHAR   bNumConfigurations;
} USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;

typedef struct _USB_CONFIGURATION_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    USHORT  wTotalLength;
    UCHAR   bNumInterfaces;
    UCHAR   bConfigurationValue;
    UCHAR   iConfiguration;
    UCHAR   bmAttributes;
    UCHAR   bMaxPower;
} USB_CONFIGURATION_DESCRIPTOR, *PUSB_CONFIGURATION_DESCRIPTOR;

typedef struct _USB_INTERFACE_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    UCHAR   bInterfaceNumber;
    UCHAR   bAlternateSetting;
    UCHAR   bNumEndpoints;
    UCHAR   bInterfaceClass;
    UCHAR   bInterfaceSubClass;
    UCHAR   bInterfaceProtocol;
    UCHAR   iInterface;
} USB_INTERFACE_DESCRIPTOR, *PUSB_INTERFACE_DESCRIPTOR;

// Типы объектов ADB
typedef enum _AdbObjectType {
    ADB_OBJECT_HANDLE = 1,
    ADB_OBJECT_INTERFACE,
    ADB_OBJECT_ENDPOINT,
    ADB_OBJECT_IO_COMPLETION
} AdbObjectType;

// Структура информации об endpoint
typedef struct _AdbEndpointInformation {
    UCHAR   bEndpointAddress;
    UCHAR   bmAttributes;
    USHORT  wMaxPacketSize;
    UCHAR   bInterval;
} AdbEndpointInformation, *PAdbEndpointInformation;

// Функции AdbWinApi.dll
typedef HRESULT (WINAPI *AdbEnumInterfacesFn)(void);
typedef HRESULT (WINAPI *AdbNextInterfaceFn)(void*, void**);
typedef HRESULT (WINAPI *AdbGetInterfaceNameFn)(void*, void*, size_t*, size_t*, int*);
typedef HRESULT (WINAPI *AdbGetUsbDeviceDescriptorFn)(void*, USB_DEVICE_DESCRIPTOR*);
typedef HRESULT (WINAPI *AdbGetUsbConfigurationDescriptorFn)(void*, USB_CONFIGURATION_DESCRIPTOR*);
typedef HRESULT (WINAPI *AdbGetUsbInterfaceDescriptorFn)(void*, USB_INTERFACE_DESCRIPTOR*);
typedef HRESULT (WINAPI *AdbOpenEndpointFn)(void*, UCHAR, void**);
typedef HRESULT (WINAPI *AdbGetEndpointInformationFn)(void*, AdbEndpointInformation*);
typedef HRESULT (WINAPI *AdbGetEndpointInterfaceFn)(void*, UCHAR, void**);
typedef HRESULT (WINAPI *AdbGetDefaultBulkReadEndpointFn)(void*, void**);
typedef HRESULT (WINAPI *AdbGetDefaultBulkWriteEndpointFn)(void*, void**);
typedef HRESULT (WINAPI *AdbGetSerialNumberFn)(void*, wchar_t*, DWORD);
typedef HRESULT (WINAPI *AdbCloseHandleFn)(void*);
typedef HRESULT (WINAPI *AdbReadEndpointSyncFn)(void*, void*, size_t, size_t*);
typedef HRESULT (WINAPI *AdbWriteEndpointSyncFn)(void*, void*, size_t, size_t*);

class AdbWinHelper {
public:
    AdbWinHelper();
    ~AdbWinHelper();

    // Инициализация/завершение
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_hLibrary != nullptr; }

    // Перечисление устройств
    struct AdbDeviceInfo {
        std::wstring serial;
        std::wstring product;
        std::wstring manufacturer;
        USHORT vendorId;
        USHORT productId;
        bool isAdbDevice;
    };

    std::vector<AdbDeviceInfo> EnumDevices();

    // Подключение к устройству
    bool ConnectToDevice(const std::wstring& serial);
    void Disconnect();
    bool IsConnected() const { return m_connected; }

    // Получение информации об устройстве
    std::wstring GetSerialNumber();
    bool GetDeviceDescriptor(USB_DEVICE_DESCRIPTOR& desc);
    bool GetConfigurationDescriptor(USB_CONFIGURATION_DESCRIPTOR& desc);
    bool GetInterfaceDescriptor(USB_INTERFACE_DESCRIPTOR& desc);

    // Чтение/запись данных
    bool ReadEndpoint(void* buffer, size_t bufferSize, size_t* bytesRead);
    bool WriteEndpoint(const void* buffer, size_t bufferSize, size_t* bytesWritten);

private:
    HMODULE m_hLibrary;
    bool m_connected;
    void* m_interface;
    void* m_readEndpoint;
    void* m_writeEndpoint;

    // Указатели на функции
    AdbEnumInterfacesFn m_fnEnumInterfaces;
    AdbNextInterfaceFn m_fnNextInterface;
    AdbGetInterfaceNameFn m_fnGetInterfaceName;
    AdbGetUsbDeviceDescriptorFn m_fnGetUsbDeviceDescriptor;
    AdbGetUsbConfigurationDescriptorFn m_fnGetUsbConfigurationDescriptor;
    AdbGetUsbInterfaceDescriptorFn m_fnGetUsbInterfaceDescriptor;
    AdbOpenEndpointFn m_fnOpenEndpoint;
    AdbGetEndpointInformationFn m_fnGetEndpointInformation;
    AdbGetEndpointInterfaceFn m_fnGetEndpointInterface;
    AdbGetDefaultBulkReadEndpointFn m_fnGetDefaultBulkReadEndpoint;
    AdbGetDefaultBulkWriteEndpointFn m_fnGetDefaultBulkWriteEndpoint;
    AdbGetSerialNumberFn m_fnGetSerialNumber;
    AdbCloseHandleFn m_fnCloseHandle;
    AdbReadEndpointSyncFn m_fnReadEndpointSync;
    AdbWriteEndpointSyncFn m_fnWriteEndpointSync;

    // Загрузка функций
    bool LoadFunctions();

    // Поиск устройства по серийному номеру
    bool FindInterfaceBySerial(const std::wstring& serial);
};