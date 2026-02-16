#include "ClrHost.h"

#include <cwchar>

// -- GUIDs ----------------------------------------------------------------

using FnCLRCreateInstance = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

// {9280188D-0E8E-4867-B30C-7FA83884E8DE}
static const GUID sCLSID_CLRMetaHost =
    {0x9280188d, 0x0e8e, 0x4867, {0xb3, 0x0c, 0x7f, 0xa8, 0x38, 0x84, 0xe8, 0xde}};

// {D332DB9E-B9B3-4125-8207-A14884F53216}
static const GUID sIID_ICLRMetaHost =
    {0xD332DB9E, 0xB9B3, 0x4125, {0x82, 0x07, 0xA1, 0x48, 0x84, 0xF5, 0x32, 0x16}};

// {BD39D1D2-BA2F-486A-89B0-B4B0CB466891}
static const GUID sIID_ICLRRuntimeInfo =
    {0xBD39D1D2, 0xBA2F, 0x486a, {0x89, 0xB0, 0xB4, 0xB0, 0xCB, 0x46, 0x68, 0x91}};

// {90F1A06E-7712-4762-86B5-7A5EBA6BDB02}
static const GUID sCLSID_CLRRuntimeHost =
    {0x90F1A06E, 0x7712, 0x4762, {0x86, 0xB5, 0x7A, 0x5E, 0xBA, 0x6B, 0xDB, 0x02}};

// {90F1A06C-7712-4762-86B5-7A5EBA6BDB02}
static const GUID sIID_ICLRRuntimeHost =
    {0x90F1A06C, 0x7712, 0x4762, {0x86, 0xB5, 0x7A, 0x5E, 0xBA, 0x6B, 0xDB, 0x02}};

// -- ClrHost implementation -----------------------------------------------

ClrHost::ClrHost()
{
    startClr();
}

ClrHost::~ClrHost()
{
    if (m_runtimeHost) m_runtimeHost->Release();
    if (m_runtimeInfo) m_runtimeInfo->Release();
    if (m_metaHost)    m_metaHost->Release();
    if (m_mscoree)     FreeLibrary(m_mscoree);
}

bool ClrHost::startClr()
{
    m_mscoree = LoadLibraryW(L"mscoree.dll");
    if (!m_mscoree)
        return false;

    auto fnCreate = reinterpret_cast<FnCLRCreateInstance>(
        GetProcAddress(m_mscoree, "CLRCreateInstance"));
    if (!fnCreate)
        return false;

    HRESULT hr = fnCreate(sCLSID_CLRMetaHost, sIID_ICLRMetaHost,
                          reinterpret_cast<LPVOID*>(&m_metaHost));
    if (FAILED(hr) || !m_metaHost)
        return false;

    hr = m_metaHost->GetRuntime(L"v4.0.30319", sIID_ICLRRuntimeInfo,
                                reinterpret_cast<LPVOID*>(&m_runtimeInfo));
    if (FAILED(hr) || !m_runtimeInfo)
        return false;

    hr = m_runtimeInfo->GetInterface(sCLSID_CLRRuntimeHost, sIID_ICLRRuntimeHost,
                                     (LPVOID*)&m_runtimeHost);
    if (FAILED(hr) || !m_runtimeHost)
        return false;

    hr = m_runtimeHost->Start();
    if (FAILED(hr))
        return false;

    m_clrStarted = true;

    return true;
}

bool ClrHost::loadManagedPlugin(const QString& bridgeDllPath,
                                const QString& pluginPath,
                                RcNetFunctions* outFunctions,
                                QString* errorMsg)
{
    if (!m_runtimeHost || !m_clrStarted) {
        if (errorMsg)
            *errorMsg = QStringLiteral(
                ".NET Framework 4.x is not available on this machine.\n"
                "Install the .NET Framework 4.7.2+ runtime to load managed plugins.");
        return false;
    }


    // Zero the function table -- the bridge will fill it
    memset(outFunctions, 0, sizeof(RcNetFunctions));

    // Build the argument string: "<hex_address_of_function_table>|<plugin_path>"
    // Use %ls (not %s) for wide strings -- MinGW follows POSIX conventions.
    wchar_t arg[2048];
    swprintf(arg, sizeof(arg) / sizeof(wchar_t),
             L"%llx|%ls",
             reinterpret_cast<unsigned long long>(outFunctions),
             reinterpret_cast<const wchar_t*>(pluginPath.utf16()));

    DWORD retVal = 0;
    HRESULT hr = m_runtimeHost->ExecuteInDefaultAppDomain(
        reinterpret_cast<LPCWSTR>(bridgeDllPath.utf16()),
        L"RcNetBridge.Bridge",
        L"Initialize",
        arg,
        &retVal
        );

    if (FAILED(hr)) {
        if (errorMsg)
            *errorMsg = QStringLiteral(
                "Failed to execute .NET bridge (HRESULT 0x%1).\n"
                "Bridge: %2\n"
                "Plugin: %3")
                .arg(static_cast<uint>(hr), 8, 16, QChar('0'))
                .arg(bridgeDllPath)
                .arg(pluginPath);
        return false;
    }

    if (retVal != 0) {
        if (errorMsg) {
            switch (retVal) {
            case 1:
                *errorMsg = QStringLiteral("Bridge: invalid argument format.");
                break;
            case 2:
                *errorMsg = QStringLiteral(
                    "No ICoreProcessFunctions implementation found in the .NET plugin.\n"
                    "The DLL may not be a ReClass.NET plugin.");
                break;
            case 3:
                *errorMsg = QStringLiteral(
                    "Failed to load the .NET plugin assembly.\n"
                    "Check that all its dependencies are available.");
                break;
            default:
                *errorMsg = QStringLiteral("Bridge returned error code %1.").arg(retVal);
                break;
            }
        }
        return false;
    }

    // Verify the bridge wrote at least the minimum required function pointers
    if (!outFunctions->ReadRemoteMemory ||
        !outFunctions->OpenRemoteProcess ||
        !outFunctions->EnumerateProcesses ||
        !outFunctions->CloseRemoteProcess) {
        if (errorMsg)
            *errorMsg = QStringLiteral(
                "The .NET bridge loaded but did not provide the required functions "
                "(ReadRemoteMemory, OpenRemoteProcess, CloseRemoteProcess, EnumerateProcesses).");
        return false;
    }

    return true;
}
