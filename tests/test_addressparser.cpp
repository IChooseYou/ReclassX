#include "addressparser.h"
#include <QTest>

using rcx::AddressParser;
using rcx::AddressParserCallbacks;
using rcx::AddressParseResult;

class TestAddressParser : public QObject {
    Q_OBJECT

private slots:
    // -- Hex literals --

    void bareHex()      { auto r = AddressParser::evaluate("AB");          QVERIFY(r.ok); QCOMPARE(r.value, 0xABULL); }
    void prefixedHex()  { auto r = AddressParser::evaluate("0x1F4");       QVERIFY(r.ok); QCOMPARE(r.value, 0x1F4ULL); }
    void zeroLiteral()  { auto r = AddressParser::evaluate("0");           QVERIFY(r.ok); QCOMPARE(r.value, 0ULL); }
    void large64bit()   { auto r = AddressParser::evaluate("7FF66CCE0000");QVERIFY(r.ok); QCOMPARE(r.value, 0x7FF66CCE0000ULL); }

    // -- Arithmetic --

    void addition() {
        auto r = AddressParser::evaluate("0x100 + 0x200");
        QVERIFY(r.ok); QCOMPARE(r.value, 0x300ULL);
    }
    void subtraction() {
        auto r = AddressParser::evaluate("0x300 - 0x100");
        QVERIFY(r.ok); QCOMPARE(r.value, 0x200ULL);
    }
    void multiplication() {
        auto r = AddressParser::evaluate("0x10 * 4");
        QVERIFY(r.ok); QCOMPARE(r.value, 0x40ULL);
    }
    void division() {
        auto r = AddressParser::evaluate("0x100 / 2");
        QVERIFY(r.ok); QCOMPARE(r.value, 0x80ULL);
    }
    void precedence() {
        // 0x10 + 2*3 = 0x10 + 6 = 0x16
        auto r = AddressParser::evaluate("0x10 + 2 * 3");
        QVERIFY(r.ok); QCOMPARE(r.value, 0x16ULL);
    }
    void parentheses() {
        // (0x10 + 2) * 3 = 0x12 * 3 = 0x36
        auto r = AddressParser::evaluate("(0x10 + 2) * 3");
        QVERIFY(r.ok); QCOMPARE(r.value, 0x36ULL);
    }

    // -- Unary minus --

    void unaryMinus() {
        auto r = AddressParser::evaluate("-0x10 + 0x20");
        QVERIFY(r.ok); QCOMPARE(r.value, 0x10ULL);
    }

    // -- Module resolution --

    void moduleResolve() {
        AddressParserCallbacks cbs;
        cbs.resolveModule = [](const QString& name, bool* ok) -> uint64_t {
            *ok = (name == "Program.exe");
            return *ok ? 0x140000000ULL : 0;
        };
        auto r = AddressParser::evaluate("<Program.exe> + 0x123", 8, &cbs);
        QVERIFY(r.ok);
        QCOMPARE(r.value, 0x140000123ULL);
    }

    void moduleNotFound() {
        AddressParserCallbacks cbs;
        cbs.resolveModule = [](const QString&, bool* ok) -> uint64_t {
            *ok = false;
            return 0;
        };
        auto r = AddressParser::evaluate("<NoSuch.dll>", 8, &cbs);
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("not found"));
    }

    // -- Dereference --

    void derefSimple() {
        AddressParserCallbacks cbs;
        cbs.readPointer = [](uint64_t addr, bool* ok) -> uint64_t {
            *ok = (addr == 0x1000);
            return *ok ? 0xDEADBEEFULL : 0;
        };
        auto r = AddressParser::evaluate("[0x1000]", 8, &cbs);
        QVERIFY(r.ok);
        QCOMPARE(r.value, 0xDEADBEEFULL);
    }

    void derefNested() {
        AddressParserCallbacks cbs;
        cbs.resolveModule = [](const QString& name, bool* ok) -> uint64_t {
            *ok = (name == "mod");
            return *ok ? 0x400000ULL : 0;
        };
        cbs.readPointer = [](uint64_t addr, bool* ok) -> uint64_t {
            *ok = true;
            if (addr == 0x400100) return 0x500000;
            if (addr == 0x900000) return 0xABCDEF;
            return 0;
        };
        // [<mod> + [<mod> + 0x100]] = [0x400000 + [0x400000+0x100]]
        //   inner deref: [0x400100] = 0x500000
        //   outer: [0x400000 + 0x500000] = [0x900000] = 0xABCDEF
        auto r = AddressParser::evaluate("[<mod> + [<mod> + 0x100]]", 8, &cbs);
        QVERIFY(r.ok);
        QCOMPARE(r.value, 0xABCDEFULL);
    }

    void derefReadFailure() {
        AddressParserCallbacks cbs;
        cbs.readPointer = [](uint64_t, bool* ok) -> uint64_t {
            *ok = false;
            return 0;
        };
        auto r = AddressParser::evaluate("[0x1000]", 8, &cbs);
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("failed to read"));
    }

    // -- Complex expression from plan --

    void complexExpr() {
        AddressParserCallbacks cbs;
        cbs.resolveModule = [](const QString& name, bool* ok) -> uint64_t {
            *ok = (name == "Program.exe");
            return *ok ? 0x140000000ULL : 0;
        };
        cbs.readPointer = [](uint64_t addr, bool* ok) -> uint64_t {
            *ok = true;
            if (addr == 0x1400000DEULL) return 0x500000;
            return 0;
        };
        // [<Program.exe> + 0xDE] - AB = [0x1400000DE] - 0xAB = 0x500000 - 0xAB = 0x4FFF55
        auto r = AddressParser::evaluate("[<Program.exe> + 0xDE] - AB", 8, &cbs);
        QVERIFY(r.ok);
        QCOMPARE(r.value, 0x4FFF55ULL);
    }

    // -- Errors --

    void emptyInput() {
        auto r = AddressParser::evaluate("");
        QVERIFY(!r.ok);
    }
    void unmatchedBracket() {
        auto r = AddressParser::evaluate("[0x1000");
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("']'"));
    }
    void unmatchedAngle() {
        auto r = AddressParser::evaluate("<Program.exe");
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("'>'"));
    }
    void divisionByZero() {
        auto r = AddressParser::evaluate("0x100 / 0");
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("division by zero"));
    }
    void trailingGarbage() {
        auto r = AddressParser::evaluate("0x100 xyz");
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("unexpected"));
    }
    void trailingOperator() {
        auto r = AddressParser::evaluate("0x100 +");
        QVERIFY(!r.ok);
    }

    // -- Validation --

    void validateValid() {
        QCOMPARE(AddressParser::validate("0x100 + 0x200"), QString());
        QCOMPARE(AddressParser::validate("<Prog.exe> + [0x100]"), QString());
    }
    void validateInvalid() {
        QVERIFY(!AddressParser::validate("").isEmpty());
        QVERIFY(!AddressParser::validate("[0x100").isEmpty());
        QVERIFY(!AddressParser::validate("0x100 xyz").isEmpty());
    }

    // -- Backtick stripping --

    void backtickStripping() {
        auto r = AddressParser::evaluate("7ff6`6cce0000");
        QVERIFY(r.ok);
        QCOMPARE(r.value, 0x7FF66CCE0000ULL);
    }

    // -- Whitespace tolerance --

    void whitespace() {
        auto r = AddressParser::evaluate("  0x100  +  0x200  ");
        QVERIFY(r.ok);
        QCOMPARE(r.value, 0x300ULL);
    }

    // -- Legacy compat: simple hex --

    void simpleHexAddress() {
        auto r = AddressParser::evaluate("140000000");
        QVERIFY(r.ok);
        QCOMPARE(r.value, 0x140000000ULL);
    }

    // -- Multiple additions --

    void multipleAdditions() {
        auto r = AddressParser::evaluate("0x100 + 0x200 + 0x300");
        QVERIFY(r.ok);
        QCOMPARE(r.value, 0x600ULL);
    }
};

QTEST_GUILESS_MAIN(TestAddressParser)
#include "test_addressparser.moc"
