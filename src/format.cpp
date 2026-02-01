#include "core.h"
#include <cstring>
#include <limits>

namespace rcx::fmt {

// ── Column layout ──
// COL_TYPE and COL_NAME use shared constants from core.h (kColType, kColName)
static constexpr int COL_TYPE  = kColType;
static constexpr int COL_NAME  = kColName;
static constexpr int COL_VALUE = 22;
static const QString SEP = QStringLiteral("  ");

static QString fit(QString s, int w) {
    if (w <= 0) return {};
    if (s.size() > w) {
        if (w >= 2) s = s.left(w - 1) + QChar(0x2026); // ellipsis
        else s = s.left(w);
    }
    return s.leftJustified(w, ' ');
}

// ── Type name ──

// Override seam: injectable type-name provider
static TypeNameFn g_typeNameFn = nullptr;

void setTypeNameProvider(TypeNameFn fn) { g_typeNameFn = fn; }

QString typeName(NodeKind kind) {
    if (g_typeNameFn) return fit(g_typeNameFn(kind), COL_TYPE);
    auto* m = kindMeta(kind);
    return fit(m ? QString::fromLatin1(m->typeName) : QStringLiteral("???"), COL_TYPE);
}

// ── Value formatting ──

static QString hexStr(uint64_t v, int digits) {
    return QStringLiteral("0x") + QString::number(v, 16).toUpper().rightJustified(digits, '0');
}

static QString rawHex(uint64_t v, int digits) {
    return QString::number(v, 16).toUpper().rightJustified(digits, '0');
}

QString fmtInt8(int8_t v)     { return QString::number(v); }
QString fmtInt16(int16_t v)   { return QString::number(v); }
QString fmtInt32(int32_t v)   { return QString::number(v); }
QString fmtInt64(int64_t v)   { return QString::number(v); }
QString fmtUInt8(uint8_t v)   { return QString::number(v); }
QString fmtUInt16(uint16_t v) { return QString::number(v); }
QString fmtUInt32(uint32_t v) { return QString::number(v); }
QString fmtUInt64(uint64_t v) { return QString::number(v); }

QString fmtFloat(float v)     { return QString::number(v, 'f', 3); }
QString fmtDouble(double v)   { return QString::number(v, 'f', 6); }
QString fmtBool(uint8_t v)    { return v ? QStringLiteral("true") : QStringLiteral("false"); }

QString fmtPointer32(uint32_t v) {
    if (v == 0) return QStringLiteral("-> NULL");
    return QStringLiteral("-> ") + hexStr(v, 8);
}

QString fmtPointer64(uint64_t v) {
    if (v == 0) return QStringLiteral("-> NULL");
    return QStringLiteral("-> ") + hexStr(v, 16);
}

// ── Indentation ──

QString indent(int depth) {
    return QString(depth * 3, ' ');
}

// ── Offset margin ──

QString fmtOffsetMargin(int64_t relativeOffset, bool isContinuation) {
    if (isContinuation) return QStringLiteral("  \u00B7");
    return QStringLiteral("+0x") + QString::number(relativeOffset, 16).toUpper();
}

// ── Struct header / footer ──

QString fmtStructHeader(const Node& node, int depth) {
    return indent(depth) + typeName(node.kind).trimmed() +
           QStringLiteral(" ") + node.name + QStringLiteral(" {");
}

QString fmtStructFooter(const Node& node, int depth, int totalSize) {
    QString s = indent(depth) + QStringLiteral("}; // ") + node.name;
    if (totalSize > 0)
        s += QStringLiteral("  sizeof=0x") + QString::number(totalSize, 16).toUpper();
    return s;
}

// ── Hex / ASCII preview ──

static inline bool isAsciiPrintable(uint8_t c) { return c >= 0x20 && c <= 0x7E; }

static QString bytesToAscii(const QByteArray& b, int slot) {
    QString out;
    out.reserve(slot);
    for (int i = 0; i < slot; ++i) {
        uint8_t c = (i < b.size()) ? (uint8_t)b[i] : 0;
        out += isAsciiPrintable(c) ? QChar(c) : QChar('.');
    }
    return out;
}

static QString bytesToHex(const QByteArray& b, int slot) {
    QString out;
    out.reserve(slot * 3);
    for (int i = 0; i < slot; ++i) {
        uint8_t c = (i < b.size()) ? (uint8_t)b[i] : 0;
        out += QString::asprintf("%02X", (unsigned)c);
        if (i + 1 < slot) out += ' ';
    }
    return out;
}

static QString fmtAsciiAndBytes(const Provider& prov, uint64_t addr,
                                int sizeBytes, int slotBytes = 8) {
    const int slot = qMax(slotBytes, sizeBytes);
    QByteArray b = prov.isReadable(addr, slot)
                       ? prov.readBytes(addr, slot)
                       : QByteArray(slot, '\0');
    return bytesToAscii(b, slot) + QStringLiteral("  ") + bytesToHex(b, slot);
}

// ── Single value from provider (unified) ──

enum class ValueMode { Display, Editable };

static QString readValueImpl(const Node& node, const Provider& prov,
                             uint64_t addr, int subLine, ValueMode mode) {
    const bool display = (mode == ValueMode::Display);
    switch (node.kind) {
    case NodeKind::Hex8:      return display ? hexStr(prov.readU8(addr), 2)  : rawHex(prov.readU8(addr), 2);
    case NodeKind::Hex16:     return display ? hexStr(prov.readU16(addr), 4) : rawHex(prov.readU16(addr), 4);
    case NodeKind::Hex32:     return display ? hexStr(prov.readU32(addr), 8) : rawHex(prov.readU32(addr), 8);
    case NodeKind::Hex64:     return display ? hexStr(prov.readU64(addr), 16): rawHex(prov.readU64(addr), 16);
    case NodeKind::Int8:      return fmtInt8((int8_t)prov.readU8(addr));
    case NodeKind::Int16:     return fmtInt16((int16_t)prov.readU16(addr));
    case NodeKind::Int32:     return fmtInt32((int32_t)prov.readU32(addr));
    case NodeKind::Int64:     return fmtInt64((int64_t)prov.readU64(addr));
    case NodeKind::UInt8:     return fmtUInt8(prov.readU8(addr));
    case NodeKind::UInt16:    return fmtUInt16(prov.readU16(addr));
    case NodeKind::UInt32:    return fmtUInt32(prov.readU32(addr));
    case NodeKind::UInt64:    return fmtUInt64(prov.readU64(addr));
    case NodeKind::Float:     { auto s = fmtFloat(prov.readF32(addr));   return display ? s : s.trimmed(); }
    case NodeKind::Double:    { auto s = fmtDouble(prov.readF64(addr));  return display ? s : s.trimmed(); }
    case NodeKind::Bool:      return fmtBool(prov.readU8(addr));
    case NodeKind::Pointer32: return display ? fmtPointer32(prov.readU32(addr)) : rawHex(prov.readU32(addr), 8);
    case NodeKind::Pointer64: return display ? fmtPointer64(prov.readU64(addr)) : rawHex(prov.readU64(addr), 16);
    case NodeKind::Vec2:
    case NodeKind::Vec3:
    case NodeKind::Vec4: {
        int maxSub = linesForKind(node.kind);
        if (subLine < 0 || subLine >= maxSub) return QStringLiteral("?");
        float component = prov.readF32(addr + subLine * 4);
        if (!display) return fmtFloat(component).trimmed();
        static const char* labels[] = {"x", "y", "z", "w"};
        return QString(labels[subLine]) + QStringLiteral(" = ") + fmtFloat(component);
    }
    case NodeKind::Mat4x4: {
        if (!display) return {};  // not editable as single value
        if (subLine < 0 || subLine >= 4) return QStringLiteral("?");
        QString line = QStringLiteral("[");
        for (int c = 0; c < 4; c++) {
            if (c > 0) line += QStringLiteral(", ");
            line += fmtFloat(prov.readF32(addr + (subLine * 4 + c) * 4)).trimmed();
        }
        line += QStringLiteral("]");
        return line;
    }
    case NodeKind::Padding:   return display ? hexStr(prov.readU8(addr), 2) : rawHex(prov.readU8(addr), 2);
    case NodeKind::UTF8: {
        QByteArray bytes = prov.readBytes(addr, node.strLen);
        int end = bytes.indexOf('\0');
        if (end >= 0) bytes.truncate(end);
        QString s = QString::fromUtf8(bytes);
        return display ? (QStringLiteral("\"") + s + QStringLiteral("\"")) : s;
    }
    case NodeKind::UTF16: {
        QByteArray bytes = prov.readBytes(addr, node.strLen * 2);
        QString s = QString::fromUtf16(reinterpret_cast<const char16_t*>(bytes.data()),
                                       bytes.size() / 2);
        int end = s.indexOf(QChar(0));
        if (end >= 0) s.truncate(end);
        return display ? (QStringLiteral("L\"") + s + QStringLiteral("\"")) : s;
    }
    default:
        return {};
    }
}

QString readValue(const Node& node, const Provider& prov,
                  uint64_t addr, int subLine) {
    return readValueImpl(node, prov, addr, subLine, ValueMode::Display);
}

// ── Full node line ──

QString fmtNodeLine(const Node& node, const Provider& prov,
                    uint64_t addr, int depth, int subLine) {
    QString ind = indent(depth);
    QString type = typeName(node.kind);
    QString name = fit(node.name, COL_NAME);
    // Blank prefix for continuation lines (same width as type+sep+name+sep)
    const int prefixW = COL_TYPE + COL_NAME + 4; // 2 seps × 2 chars

    // Mat4x4: subLine 0..3 = rows
    if (node.kind == NodeKind::Mat4x4) {
        QString val = readValue(node, prov, addr, subLine);
        if (subLine == 0) return ind + type + SEP + name + SEP + val;
        return ind + QString(prefixW, ' ') + val;
    }

    // For vector types, subLine selects component
    if (subLine > 0 && (node.kind == NodeKind::Vec2 ||
                        node.kind == NodeKind::Vec3 ||
                        node.kind == NodeKind::Vec4)) {
        QString val = readValue(node, prov, addr, subLine);
        return ind + QString(prefixW, ' ') + val;
    }

    // Hex nodes and Padding: ASCII preview + hex bytes (compact)
    if (node.kind == NodeKind::Hex8  || node.kind == NodeKind::Hex16 ||
        node.kind == NodeKind::Hex32 || node.kind == NodeKind::Hex64 ||
        node.kind == NodeKind::Padding)
    {
        if (node.kind == NodeKind::Padding) {
            const int totalSz = qMax(1, node.arrayLen);
            const int lineOff = subLine * 8;
            const int lineBytes = qMin(8, totalSz - lineOff);
            QByteArray b = prov.isReadable(addr + lineOff, lineBytes)
                ? prov.readBytes(addr + lineOff, lineBytes) : QByteArray(lineBytes, '\0');
            QString ascii = bytesToAscii(b, lineBytes);
            QString hex = bytesToHex(b, lineBytes);
            if (subLine == 0)
                return ind + type + SEP + ascii + SEP + hex;
            return ind + QString(COL_TYPE + (int)SEP.size(), ' ') + ascii + SEP + hex;
        }
        // Hex8..Hex64: single line, ASCII padded to 8 chars so hex column aligns
        const int sz = sizeForKind(node.kind);
        QByteArray b = prov.isReadable(addr, sz)
            ? prov.readBytes(addr, sz) : QByteArray(sz, '\0');
        QString ascii = bytesToAscii(b, sz).leftJustified(8, ' ');
        QString hex = bytesToHex(b, sz);
        return ind + type + SEP + ascii + SEP + hex;
    }

    QString val = readValue(node, prov, addr, subLine);
    return ind + type + SEP + name + SEP + val;
}

// ── Editable value (parse-friendly form for edit dialog) ──

QString editableValue(const Node& node, const Provider& prov,
                      uint64_t addr, int subLine) {
    return readValueImpl(node, prov, addr, subLine, ValueMode::Editable);
}

// ── Value parsing (text → bytes) ──

template<class T>
static QByteArray toBytes(T v) {
    QByteArray b(sizeof(T), Qt::Uninitialized);
    memcpy(b.data(), &v, sizeof(T));
    return b;
}

static QString stripHex(const QString& s) {
    if (s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        return s.mid(2);
    return s;
}

// Range-checked narrowing: sets *ok = false if parsed value doesn't fit in T
template<class T, class ParseT>
static QByteArray parseIntChecked(ParseT val, bool* ok) {
    if (*ok) {
        using L = std::numeric_limits<T>;
        if constexpr (std::is_signed_v<T>) {
            if (val < (ParseT)L::min() || val > (ParseT)L::max()) *ok = false;
        } else {
            if (val > (ParseT)L::max()) *ok = false;
        }
    }
    return *ok ? toBytes<T>(static_cast<T>(val)) : QByteArray{};
}

QByteArray parseValue(NodeKind kind, const QString& text, bool* ok) {
    *ok = false;
    QString s = text.trimmed();

    // Allow empty for string types (will produce zero-length content, caller pads)
    if (s.isEmpty()) {
        if (kind == NodeKind::UTF8 || kind == NodeKind::UTF16) {
            *ok = true;
            return {};
        }
        return {};
    }

    switch (kind) {
    case NodeKind::Hex8:    { uint val = stripHex(s).remove(' ').toUInt(ok, 16);     return parseIntChecked<uint8_t>(val, ok); }
    case NodeKind::Hex16:   { uint val = stripHex(s).remove(' ').toUInt(ok, 16);     return parseIntChecked<uint16_t>(val, ok); }
    case NodeKind::Hex32:   { uint val = stripHex(s).remove(' ').toUInt(ok, 16);     return *ok ? toBytes<uint32_t>(val) : QByteArray{}; }
    case NodeKind::Hex64:   { qulonglong val = stripHex(s).remove(' ').toULongLong(ok, 16); return *ok ? toBytes<uint64_t>(val) : QByteArray{}; }
    case NodeKind::Int8:    { int val = s.toInt(ok);                      return parseIntChecked<int8_t>(val, ok); }
    case NodeKind::Int16:   { int val = s.toInt(ok);                      return parseIntChecked<int16_t>(val, ok); }
    case NodeKind::Int32:   { int val = s.toInt(ok);                      return *ok ? toBytes<int32_t>(val) : QByteArray{}; }
    case NodeKind::Int64:   { qlonglong val = s.toLongLong(ok);           return *ok ? toBytes<int64_t>(val) : QByteArray{}; }
    case NodeKind::UInt8:   { uint val = s.toUInt(ok);                    return parseIntChecked<uint8_t>(val, ok); }
    case NodeKind::UInt16:  { uint val = s.toUInt(ok);                    return parseIntChecked<uint16_t>(val, ok); }
    case NodeKind::UInt32:  { uint val = s.toUInt(ok);                    return *ok ? toBytes<uint32_t>(val) : QByteArray{}; }
    case NodeKind::UInt64:  { qulonglong val = s.toULongLong(ok);         return *ok ? toBytes<uint64_t>(val) : QByteArray{}; }
    case NodeKind::Float: {
        float val = s.toFloat(ok);
        return *ok ? toBytes<float>(val) : QByteArray{};
    }
    case NodeKind::Double: {
        double val = s.toDouble(ok);
        return *ok ? toBytes<double>(val) : QByteArray{};
    }
    case NodeKind::Bool: {
        if (s == QStringLiteral("true") || s == QStringLiteral("1")) {
            *ok = true; return toBytes<uint8_t>(1);
        }
        if (s == QStringLiteral("false") || s == QStringLiteral("0")) {
            *ok = true; return toBytes<uint8_t>(0);
        }
        return {};  // unknown token → ok stays false
    }
    case NodeKind::Pointer32: {
        uint val = stripHex(s).toUInt(ok, 16);
        return *ok ? toBytes<uint32_t>(val) : QByteArray{};
    }
    case NodeKind::Pointer64: {
        qulonglong val = stripHex(s).toULongLong(ok, 16);
        return *ok ? toBytes<uint64_t>(val) : QByteArray{};
    }
    case NodeKind::UTF8: {
        *ok = true;
        if (s.startsWith('"') && s.endsWith('"'))
            s = s.mid(1, s.size() - 2);
        return s.toUtf8();
    }
    case NodeKind::UTF16: {
        *ok = true;
        if (s.startsWith(QStringLiteral("L\""))) s = s.mid(2);
        else if (s.startsWith('"')) s = s.mid(1);
        if (s.endsWith('"')) s.chop(1);
        QByteArray b(s.size() * 2, Qt::Uninitialized);
        memcpy(b.data(), s.utf16(), s.size() * 2);
        return b;
    }
    default:
        return {};
    }
}

} // namespace rcx::fmt
