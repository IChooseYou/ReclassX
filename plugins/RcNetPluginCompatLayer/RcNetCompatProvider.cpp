#include "RcNetCompatProvider.h"

#include <QFileInfo>
#include <cstring>

// -- Construction / destruction -------------------------------------------

RcNetCompatProvider::RcNetCompatProvider(const RcNetFunctions& fns,
                                         uint32_t pid,
                                         const QString& processName)
    : m_fns(fns)
    , m_pid(pid)
    , m_processName(processName)
{
    if (m_fns.OpenRemoteProcess)
        m_handle = m_fns.OpenRemoteProcess(static_cast<RC_Size>(pid),
                                           ProcessAccess::Full);

    if (m_handle)
        cacheModules();
}

RcNetCompatProvider::~RcNetCompatProvider()
{
    if (m_handle && m_fns.CloseRemoteProcess)
        m_fns.CloseRemoteProcess(m_handle);
}

// -- Required overrides ---------------------------------------------------

bool RcNetCompatProvider::read(uint64_t addr, void* buf, int len) const
{
    if (!m_handle || !m_fns.ReadRemoteMemory || len <= 0)
        return false;

    return m_fns.ReadRemoteMemory(m_handle,
                                  reinterpret_cast<RC_Pointer>(addr),
                                  static_cast<RC_Pointer>(buf),
                                  0, len);
}

int RcNetCompatProvider::size() const
{
    if (!m_handle) return 0;
    if (m_fns.IsProcessValid && !m_fns.IsProcessValid(m_handle)) return 0;
    return 0x10000;
}

// -- Optional overrides ---------------------------------------------------

bool RcNetCompatProvider::write(uint64_t addr, const void* buf, int len)
{
    if (!m_handle || !m_fns.WriteRemoteMemory || len <= 0)
        return false;

    return m_fns.WriteRemoteMemory(m_handle,
                                   reinterpret_cast<RC_Pointer>(addr),
                                   const_cast<RC_Pointer>(static_cast<const void*>(buf)),
                                   0, len);
}

QString RcNetCompatProvider::getSymbol(uint64_t addr) const
{
    for (const auto& mod : m_modules)
    {
        if (addr >= mod.base && addr < mod.base + mod.size)
        {
            uint64_t offset = addr - mod.base;
            return QStringLiteral("%1+0x%2")
                .arg(mod.name)
                .arg(offset, 0, 16, QChar('0'));
        }
    }
    return {};
}

uint64_t RcNetCompatProvider::symbolToAddress(const QString& name) const
{
    for (const auto& mod : m_modules) {
        if (mod.name.compare(name, Qt::CaseInsensitive) == 0)
            return mod.base;
    }
    return 0;
}

// -- Module enumeration ---------------------------------------------------

namespace {

// Thread-local collector for the module enumeration callback.
// ReClass.NET callbacks are synchronous, so this is safe.
struct ModuleCollector {
    QVector<RcNetCompatProvider::ModuleInfo>* dest = nullptr;
};
thread_local ModuleCollector g_moduleCollector;

void RC_CALLCONV moduleCallback(EnumerateRemoteModuleData* data)
{
    if (!data || !g_moduleCollector.dest) return;

    QString path = QString::fromUtf16(data->Path);
    QFileInfo fi(path);

    RcNetCompatProvider::ModuleInfo info;
    info.name = fi.fileName();
    info.base = reinterpret_cast<uint64_t>(data->BaseAddress);
    info.size = static_cast<uint64_t>(data->Size);
    g_moduleCollector.dest->append(info);
}

// We still need a section callback even though we don't use it.
void RC_CALLCONV sectionCallback(EnumerateRemoteSectionData*)
{
    // Intentionally empty -- we only need module data.
}

} // anonymous namespace

void RcNetCompatProvider::cacheModules()
{
    if (!m_fns.EnumerateRemoteSectionsAndModules || !m_handle)
        return;

    m_modules.clear();
    g_moduleCollector.dest = &m_modules;
    m_fns.EnumerateRemoteSectionsAndModules(m_handle, sectionCallback, moduleCallback);
    g_moduleCollector.dest = nullptr;

    // Set base to first module if we got any
    if (!m_modules.isEmpty() && m_base == 0)
        m_base = m_modules.first().base;
}
