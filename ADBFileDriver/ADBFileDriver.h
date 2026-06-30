#pragma once

#include "ComponentBase.h"
#include "AddInDefBase.h"
#include "IMemoryManager.h"
#include <string>
#include <vector>

// Имя расширения встроенного языка
#define EXTENSION_NAME L"ADBFileDriver"

// Версия компоненты
#define COMPONENT_VERSION L"1.0.0"

// Структура устройства ADB
struct ADBDevice {
    std::wstring serial;
    std::wstring status;    // "device", "offline", "unauthorized"
    std::wstring model;
};

// ============================================================
// Класс компоненты ADBFileDriver
// ============================================================
class CADBFileDriver : public IComponentBase
{
private:
    IAddInDefBase* m_pBackConn;       // Связь с 1С
    IMemoryManager* m_pMemMgr;        // Менеджер памяти 1С
    bool m_bInitialized;               // Инициализирована ли компонента

    // ADB-данные
    std::wstring m_strADBPath;         // Путь к adb.exe
    std::wstring m_strStatus;          // Текущий статус подключения
    std::wstring m_connectedDevice;    // Подключённое устройство (серийный номер)
    bool m_bConnected;                 // Подключено ли к устройству

public:
    // =====================================================
    // LocaleBase — локализация
    // =====================================================
    virtual void ADDIN_API SetLocale(const WCHAR_T* loc) override;

    // Вспомогательные методы
    void setWStringToTVariant(tVariant* dest, const wchar_t* source);
    void addError(uint32_t wcode, const wchar_t* source, const wchar_t* descriptor, long code);
    bool FindADB();
    std::wstring ExecuteADBCommand(const std::wstring& cmd);
    std::wstring ExecuteADBCommandToDevice(const std::wstring& cmd);
    // WideToAnsi удалена — не используется. Оставлена для обратной совместимости.
    // Если нужна — реализуйте как WideToUTF8String.
    std::string GetDevicesJSON();
    bool UpdateStatus(const std::wstring& status);

    // Поиск свойств/методов (вспомогательный)
    long findName(const wchar_t* names[], const wchar_t* name, const uint32_t size) const;

public:
    CADBFileDriver();
    virtual ~CADBFileDriver();

    // =====================================================
    // IComponentBase
    // =====================================================
    virtual bool ADDIN_API Init(void* Interface) override;
    virtual void ADDIN_API Done() override;
    virtual long ADDIN_API GetInfo() override;
    virtual bool ADDIN_API setMemManager(void* memManager) override;

    // =====================================================
    // ILanguageExtender — Свойства
    // =====================================================
    virtual long ADDIN_API GetNProps() override;
    virtual long ADDIN_API FindProp(const WCHAR_T* wsPropName) override;
    virtual const WCHAR_T* ADDIN_API GetPropName(long lPropNum, long lPropAlias) override;
    virtual bool ADDIN_API GetPropVal(const long lPropNum, tVariant* pvarPropVal) override;
    virtual bool ADDIN_API SetPropVal(const long lPropNum, tVariant* pvarPropVal) override;
    virtual bool ADDIN_API IsPropReadable(const long lPropNum) override;
    virtual bool ADDIN_API IsPropWritable(const long lPropNum) override;

    // =====================================================
    // ILanguageExtender — Методы
    // =====================================================
    virtual long ADDIN_API GetNMethods() override;
    virtual long ADDIN_API FindMethod(const WCHAR_T* wsMethodName) override;
    virtual const WCHAR_T* ADDIN_API GetMethodName(const long lMethodNum, const long lMethodAlias) override;
    virtual long ADDIN_API GetNParams(const long lMethodNum) override;
    virtual bool ADDIN_API GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant* pvarParamDefValue) override;
    virtual bool ADDIN_API HasRetVal(const long lMethodNum) override;
    virtual bool ADDIN_API CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray) override;
    virtual bool ADDIN_API CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray) override;

    // =====================================================
    // ILanguageExtender — Регистрация
    // =====================================================
    virtual bool ADDIN_API RegisterExtensionAs(WCHAR_T** wsExtName) override;

    // =====================================================
    // Реализация свойств
    // =====================================================
    bool GetProp_Status(tVariant* pvarPropVal);
    bool GetProp_DeviceList(tVariant* pvarPropVal);

    // =====================================================
    // Реализация методов
    // =====================================================
    bool Method_Connect(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_Disconnect(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_PushFile(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_PullText(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
    bool Method_GetDeviceList(tVariant* pvarRetValue, tVariant* paParams, long lSizeArray);
};

// ============================================================
// Экспортируемые функции DLL (реализация в .cpp)
// ============================================================
// Примечание: сигнатуры этих функций определены в ComponentBase.h
// Здесь они экспортируются через extern "C"
