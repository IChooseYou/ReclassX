#pragma once
#include "provider.h"
#include <memory>

namespace rcx {

// Provider that reads from a cached QByteArray snapshot but delegates
// metadata (name, kind, getSymbol) to the underlying real provider.
// Used for async refresh: worker thread reads bulk data into a snapshot,
// UI thread composes against it without blocking.
class SnapshotProvider : public Provider {
    std::shared_ptr<Provider> m_real;
    QByteArray m_data;

public:
    SnapshotProvider(std::shared_ptr<Provider> real, QByteArray snapshot)
        : m_real(std::move(real)), m_data(std::move(snapshot)) {}

    bool read(uint64_t addr, void* buf, int len) const override {
        if (!isReadable(addr, len)) return false;
        std::memcpy(buf, m_data.constData() + addr, len);
        return true;
    }

    int size() const override { return m_data.size(); }
    bool isWritable() const override { return m_real ? m_real->isWritable() : false; }
    bool isLive() const override { return m_real ? m_real->isLive() : false; }
    QString name() const override { return m_real ? m_real->name() : QString(); }
    QString kind() const override { return m_real ? m_real->kind() : QStringLiteral("File"); }
    QString getSymbol(uint64_t addr) const override {
        return m_real ? m_real->getSymbol(addr) : QString();
    }

    bool write(uint64_t addr, const void* buf, int len) override {
        if (!m_real) return false;
        bool ok = m_real->write(addr, buf, len);
        if (ok && isReadable(addr, len))
            std::memcpy(m_data.data() + addr, buf, len);
        return ok;
    }

    // Update the entire snapshot (called after async read completes)
    void updateSnapshot(QByteArray data) { m_data = std::move(data); }

    // Patch specific bytes in the snapshot (called after user writes a value)
    void patchSnapshot(uint64_t addr, const void* buf, int len) {
        if (isReadable(addr, len))
            std::memcpy(m_data.data() + addr, buf, len);
    }

    const QByteArray& snapshot() const { return m_data; }
};

} // namespace rcx
