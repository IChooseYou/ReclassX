#include "disasm.h"

extern "C" {
#include <fadec.h>
}

namespace rcx {

QString disassemble(const QByteArray& bytes, uint64_t baseAddr, int bitness, int maxBytes) {
    if (bytes.isEmpty() || (bitness != 32 && bitness != 64))
        return {};

    int len = qMin((int)bytes.size(), maxBytes);
    const auto* buf = reinterpret_cast<const uint8_t*>(bytes.constData());

    QString result;
    int off = 0;
    while (off < len) {
        FdInstr instr;
        int ret = fd_decode(buf + off, len - off, bitness, baseAddr + off, &instr);
        if (ret < 0)
            break;

        char fmtBuf[128];
        fd_format(&instr, fmtBuf, sizeof(fmtBuf));

        if (!result.isEmpty())
            result += QLatin1Char('\n');
        result += QStringLiteral("%1  %2")
            .arg(baseAddr + off, bitness == 64 ? 16 : 8, 16, QLatin1Char('0'))
            .arg(QString::fromLatin1(fmtBuf));

        off += ret;
    }
    return result;
}

QString hexDump(const QByteArray& bytes, uint64_t baseAddr, int maxBytes) {
    if (bytes.isEmpty())
        return {};

    int len = qMin((int)bytes.size(), maxBytes);
    QString result;

    for (int off = 0; off < len; off += 16) {
        int lineLen = qMin(16, len - off);

        if (!result.isEmpty())
            result += QLatin1Char('\n');

        // Address
        bool wide = (baseAddr + len > 0xFFFFFFFFULL);
        result += QStringLiteral("%1  ").arg(baseAddr + off, wide ? 16 : 8, 16, QLatin1Char('0'));

        // Hex bytes
        for (int i = 0; i < 16; i++) {
            if (i < lineLen) {
                uint8_t b = static_cast<uint8_t>(bytes[off + i]);
                result += QStringLiteral("%1 ").arg(b, 2, 16, QLatin1Char('0'));
            } else {
                result += QStringLiteral("   ");
            }
            if (i == 7) result += QLatin1Char(' ');
        }

        // ASCII
        result += QLatin1Char(' ');
        for (int i = 0; i < lineLen; i++) {
            char c = bytes[off + i];
            result += (c >= 0x20 && c < 0x7f) ? QLatin1Char(c) : QLatin1Char('.');
        }
    }
    return result;
}

} // namespace rcx
