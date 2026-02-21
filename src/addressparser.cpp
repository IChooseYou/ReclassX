#include "addressparser.h"

namespace rcx {

// ── Address Expression Parser ──────────────────────────────────────────
//
// Parses expressions like:
//   "7FF66CCE0000"                    → plain hex address
//   "0x100 + 0x200"                   → arithmetic on hex values
//   "<Program.exe> + 0xDE"            → module base + offset
//   "[<Program.exe> + 0xDE] - AB"     → dereference pointer, then subtract
//   "7ff6`6cce0000"                   → WinDbg-style backtick separator (stripped before parsing)
//
// Grammar (standard operator precedence: *, / bind tighter than +, -):
//
//   expr   = term (('+' | '-') term)*
//   term   = unary (('*' | '/') unary)*
//   unary  = '-' unary | atom
//   atom   = '[' expr ']'             -- read pointer at address (dereference)
//          | '<' moduleName '>'       -- resolve module base address
//          | '(' expr ')'             -- grouping
//          | hexLiteral               -- hex number, optional 0x prefix
//
// All numeric literals are hexadecimal (base 16).
// Module names and pointer reads are resolved via optional callbacks.
// Without callbacks, modules and dereferences evaluate to 0 (syntax-check mode).

class ExpressionParser {
public:
    ExpressionParser(const QString& input, const AddressParserCallbacks* callbacks)
        : m_input(input), m_callbacks(callbacks) {}

    AddressParseResult parse() {
        skipSpaces();
        if (atEnd())
            return error("empty expression");

        uint64_t value = 0;
        if (!parseExpression(value))
            return error(m_error);

        skipSpaces();
        if (!atEnd())
            return error(QStringLiteral("unexpected '%1'").arg(m_input[m_pos]));

        return {true, value, {}, -1};
    }

private:
    const QString& m_input;
    const AddressParserCallbacks* m_callbacks;
    int m_pos = 0;
    QString m_error;
    int m_errorPos = 0;

    // ── Helpers ──

    bool atEnd() const { return m_pos >= m_input.size(); }

    QChar peek() const { return atEnd() ? QChar('\0') : m_input[m_pos]; }

    void advance() { m_pos++; }

    void skipSpaces() {
        while (!atEnd() && m_input[m_pos].isSpace())
            m_pos++;
    }

    AddressParseResult error(const QString& msg) const {
        return {false, 0, msg, m_errorPos};
    }

    bool fail(const QString& msg) {
        m_error = msg;
        m_errorPos = m_pos;
        return false;
    }

    bool expect(QChar ch) {
        skipSpaces();
        if (peek() != ch)
            return fail(QStringLiteral("expected '%1'").arg(ch));
        advance();
        return true;
    }

    static bool isHexDigit(QChar ch) {
        return (ch >= '0' && ch <= '9')
            || (ch >= 'a' && ch <= 'f')
            || (ch >= 'A' && ch <= 'F');
    }

    // ── Recursive descent parsing ──

    // expr = term (('+' | '-') term)*
    bool parseExpression(uint64_t& result) {
        if (!parseTerm(result))
            return false;

        for (;;) {
            skipSpaces();
            QChar op = peek();
            if (op != '+' && op != '-')
                break;
            advance();

            uint64_t rhs = 0;
            if (!parseTerm(rhs))
                return false;

            result = (op == '+') ? result + rhs : result - rhs;
        }
        return true;
    }

    // term = unary (('*' | '/') unary)*
    bool parseTerm(uint64_t& result) {
        if (!parseUnary(result))
            return false;

        for (;;) {
            skipSpaces();
            QChar op = peek();
            if (op != '*' && op != '/')
                break;
            advance();

            uint64_t rhs = 0;
            if (!parseUnary(rhs))
                return false;

            if (op == '*') {
                result *= rhs;
            } else {
                if (rhs == 0)
                    return fail("division by zero");
                result /= rhs;
            }
        }
        return true;
    }

    // unary = '-' unary | atom
    bool parseUnary(uint64_t& result) {
        skipSpaces();
        if (peek() == '-') {
            advance();
            uint64_t inner = 0;
            if (!parseUnary(inner))
                return false;
            result = static_cast<uint64_t>(-static_cast<int64_t>(inner));
            return true;
        }
        return parseAtom(result);
    }

    // atom = '[' expr ']' | '<' name '>' | '(' expr ')' | hexLiteral
    bool parseAtom(uint64_t& result) {
        skipSpaces();
        if (atEnd())
            return fail("unexpected end of expression");

        QChar ch = peek();

        if (ch == '[') return parseDereference(result);
        if (ch == '<') return parseModuleName(result);
        if (ch == '(') return parseGrouping(result);
        return parseHexNumber(result);
    }

    // '[' expr ']' — read the pointer value at the computed address
    bool parseDereference(uint64_t& result) {
        advance(); // skip '['

        uint64_t address = 0;
        if (!parseExpression(address))
            return false;
        if (!expect(']'))
            return false;

        // Without a callback, just return 0 (syntax-check mode)
        if (!m_callbacks || !m_callbacks->readPointer) {
            result = 0;
            return true;
        }

        bool ok = false;
        result = m_callbacks->readPointer(address, &ok);
        if (!ok)
            return fail(QStringLiteral("failed to read memory at 0x%1").arg(address, 0, 16));
        return true;
    }

    // '<' moduleName '>' — resolve a module's base address (e.g. <Program.exe>)
    bool parseModuleName(uint64_t& result) {
        advance(); // skip '<'

        int nameStart = m_pos;
        while (!atEnd() && peek() != '>')
            advance();
        if (atEnd())
            return fail("expected '>'");

        QString name = m_input.mid(nameStart, m_pos - nameStart).trimmed();
        advance(); // skip '>'

        if (name.isEmpty())
            return fail("empty module name");

        // Without a callback, just return 0 (syntax-check mode)
        if (!m_callbacks || !m_callbacks->resolveModule) {
            result = 0;
            return true;
        }

        bool ok = false;
        result = m_callbacks->resolveModule(name, &ok);
        if (!ok)
            return fail(QStringLiteral("module '%1' not found").arg(name));
        return true;
    }

    // '(' expr ')' — parenthesized sub-expression for grouping
    bool parseGrouping(uint64_t& result) {
        advance(); // skip '('
        if (!parseExpression(result))
            return false;
        return expect(')');
    }

    // Hex number with optional "0x" prefix. All literals are base-16.
    bool parseHexNumber(uint64_t& result) {
        skipSpaces();
        if (atEnd())
            return fail("unexpected end of expression");

        int start = m_pos;

        // Skip optional 0x/0X prefix
        if (m_pos + 1 < m_input.size()
            && m_input[m_pos] == '0'
            && (m_input[m_pos + 1] == 'x' || m_input[m_pos + 1] == 'X'))
            m_pos += 2;

        // Consume hex digits
        int digitsStart = m_pos;
        while (!atEnd() && isHexDigit(peek()))
            advance();

        if (m_pos == digitsStart) {
            m_errorPos = start;
            return fail("expected hex number");
        }

        QString digits = m_input.mid(digitsStart, m_pos - digitsStart);
        bool ok = false;
        result = digits.toULongLong(&ok, 16);
        if (!ok) {
            m_errorPos = start;
            return fail("invalid hex number");
        }
        return true;
    }
};

// ── Public API ─────────────────────────────────────────────────────────

AddressParseResult AddressParser::evaluate(const QString& formula, int ptrSize,
                                           const AddressParserCallbacks* cb)
{
    Q_UNUSED(ptrSize);

    // WinDbg displays 64-bit addresses with backtick separators for readability,
    // e.g. "00007ff6`1a2b3c4d". Strip them so users can paste directly.
    // Also remove ' in case user uses it
    QString cleaned = formula;
    cleaned.remove('`');
    cleaned.remove('\'');

    ExpressionParser parser(cleaned, cb);
    return parser.parse();
}

QString AddressParser::validate(const QString& formula)
{
    QString cleaned = formula;
    cleaned.remove('`');
    cleaned.remove('\'');
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty())
        return QStringLiteral("empty");

    // Parse with no callbacks — modules and dereferences succeed but return 0.
    // This checks syntax only.
    ExpressionParser parser(cleaned, nullptr);
    auto result = parser.parse();
    return result.ok ? QString() : result.error;
}

} // namespace rcx
