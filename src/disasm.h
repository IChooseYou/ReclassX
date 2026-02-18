#pragma once
#include <QString>
#include <QByteArray>
#include <cstdint>

namespace rcx {

// Disassemble up to maxBytes of x86 code, returning formatted asm lines.
// bitness: 32 or 64. Returns one line per instruction, prefixed with offset.
QString disassemble(const QByteArray& bytes, uint64_t baseAddr, int bitness, int maxBytes = 128);

// Format bytes as hex dump lines (16 bytes per line with ASCII sidebar).
QString hexDump(const QByteArray& bytes, uint64_t baseAddr, int maxBytes = 128);

} // namespace rcx
