#pragma once
// In-process CLR hosting for loading .NET ReClass.NET plugins.
// Dynamically loads mscoree.dll and uses ICLRMetaHost -> ICLRRuntimeInfo ->
// ICLRRuntimeHost::ExecuteInDefaultAppDomain to call into the C# bridge.

#include "ReClassNET_Plugin.hpp"
#include <QString>
#include <windows.h>
#include <objbase.h>

// -- Minimal COM interface definitions for CLR hosting --------------------
// Defined here to avoid depending on Windows SDK metahost.h / mscoree.h
// which may not be present in all MinGW distributions.
// Only methods we actually call have real signatures; the rest are stubs
// that preserve correct vtable offsets.

#undef INTERFACE
#define INTERFACE ICLRMetaHost
DECLARE_INTERFACE_(ICLRMetaHost, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) PURE;
    STDMETHOD_(ULONG, AddRef)() PURE;
    STDMETHOD_(ULONG, Release)() PURE;
    // ICLRMetaHost
    STDMETHOD(GetRuntime)(LPCWSTR pwzVersion, REFIID riid, LPVOID* ppRuntime) PURE;
    STDMETHOD(GetVersionFromFile)(LPCWSTR, LPWSTR, DWORD*) PURE;
    STDMETHOD(EnumerateInstalledRuntimes)(void**) PURE;
    STDMETHOD(EnumerateLoadedRuntimes)(HANDLE, void**) PURE;
    STDMETHOD(RequestRuntimeLoadedNotification)(void*) PURE;
    STDMETHOD(QueryLegacyV2RuntimeBinding)(REFIID, LPVOID*) PURE;
    STDMETHOD_(void, ExitProcess)(INT32) PURE;
};
#undef INTERFACE

#define INTERFACE ICLRRuntimeInfo
DECLARE_INTERFACE_(ICLRRuntimeInfo, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) PURE;
    STDMETHOD_(ULONG, AddRef)() PURE;
    STDMETHOD_(ULONG, Release)() PURE;
    // ICLRRuntimeInfo
    STDMETHOD(GetVersionString)(LPWSTR, DWORD*) PURE;
    STDMETHOD(GetRuntimeDirectory)(LPWSTR, DWORD*) PURE;
    STDMETHOD(IsLoaded)(HANDLE, BOOL*) PURE;
    STDMETHOD(LoadErrorString)(UINT, LPWSTR, DWORD*, LONG) PURE;
    STDMETHOD(LoadLibrary)(LPCWSTR, HMODULE*) PURE;
    STDMETHOD(GetProcAddress)(LPCSTR, LPVOID*) PURE;
    STDMETHOD(GetInterface)(REFCLSID rclsid, REFIID riid, LPVOID* ppUnk) PURE;
};
#undef INTERFACE

#define INTERFACE ICLRRuntimeHost
DECLARE_INTERFACE_(ICLRRuntimeHost, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) PURE;
    STDMETHOD_(ULONG, AddRef)() PURE;
    STDMETHOD_(ULONG, Release)() PURE;
    // ICLRRuntimeHost
    STDMETHOD(Start)() PURE;
    STDMETHOD(Stop)() PURE;
    STDMETHOD(SetHostControl)(void*) PURE;
    STDMETHOD(GetCLRControl)(void**) PURE;
    STDMETHOD(UnloadAppDomain)(DWORD, BOOL) PURE;
    STDMETHOD(ExecuteInAppDomain)(DWORD, void*, void*) PURE;
    STDMETHOD(GetCurrentAppDomainId)(DWORD*) PURE;
    STDMETHOD(ExecuteApplication)(LPCWSTR, DWORD, LPCWSTR*, DWORD, LPCWSTR*, int*) PURE;
    STDMETHOD(ExecuteInDefaultAppDomain)(LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, DWORD*) PURE;
};
#undef INTERFACE

// -- CLR Host wrapper -----------------------------------------------------

class ClrHost
{
public:
    ClrHost();
    ~ClrHost();

    // True if the .NET Framework CLR (v4.0) is available on this machine.
    bool isAvailable() const { return m_runtimeHost != nullptr && m_clrStarted; }

    // Load a managed ReClass.NET plugin via the C# bridge.
    bool loadManagedPlugin(const QString& bridgeDllPath,
                           const QString& pluginPath,
                           RcNetFunctions* outFunctions,
                           QString* errorMsg = nullptr);

private:
    bool startClr();

    HMODULE           m_mscoree         = nullptr;
    ICLRMetaHost*     m_metaHost        = nullptr;
    ICLRRuntimeInfo*  m_runtimeInfo     = nullptr;
    ICLRRuntimeHost*  m_runtimeHost     = nullptr;
    bool              m_clrStarted      = false;
};
