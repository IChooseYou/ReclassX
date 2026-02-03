#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QApplication>
#include <QKeyEvent>
#include <QFocusEvent>
#include <Qsci/qsciscintilla.h>
#include "editor.h"
#include "core.h"

using namespace rcx;

// Minimal provider for testing
static FileProvider makeTestProvider() {
    QByteArray data(256, '\0');
    // Write known values: uint16_t=23117 at offset 0, Hex64 at offset 8
    uint16_t u16 = 23117;
    memcpy(data.data(), &u16, 2);
    uint64_t h64 = 0x4D5A900000000000ULL;
    memcpy(data.data() + 8, &h64, 8);
    return FileProvider(data);
}

// Build a simple tree with a struct containing a few fields
static NodeTree makeTestTree() {
    NodeTree tree;
    tree.baseAddress = 0;

    Node root;
    root.kind = NodeKind::Struct;
    root.name = "TestStruct";
    root.parentId = 0;
    root.offset = 0;
    int ri = tree.addNode(root);
    uint64_t rootId = tree.nodes[ri].id;

    Node f1;
    f1.kind = NodeKind::UInt16;
    f1.name = "field_u16";
    f1.parentId = rootId;
    f1.offset = 0;
    tree.addNode(f1);

    Node f2;
    f2.kind = NodeKind::Hex64;
    f2.name = "field_hex";
    f2.parentId = rootId;
    f2.offset = 8;
    tree.addNode(f2);

    return tree;
}

class TestEditor : public QObject {
    Q_OBJECT
private:
    RcxEditor* m_editor = nullptr;
    ComposeResult m_result;

private slots:
    void initTestCase() {
        m_editor = new RcxEditor();
        m_editor->resize(800, 600);
        m_editor->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_editor));

        NodeTree tree = makeTestTree();
        FileProvider prov = makeTestProvider();
        m_result = compose(tree, prov);
        m_editor->applyDocument(m_result);
    }

    void cleanupTestCase() {
        delete m_editor;
    }

    // ── Test: inline edit lifecycle (begin → commit → re-edit) ──
    void testInlineEditReEntry() {
        // Move cursor to line 1 (first field inside struct)
        m_editor->scintilla()->setCursorPosition(1, 0);

        // Should not be editing
        QVERIFY(!m_editor->isEditing());

        // Begin edit on Name column
        bool ok = m_editor->beginInlineEdit(EditTarget::Name, 1);
        QVERIFY(ok);
        QVERIFY(m_editor->isEditing());

        // Cancel the edit
        m_editor->cancelInlineEdit();
        QVERIFY(!m_editor->isEditing());

        // Re-apply document (simulates controller refresh)
        m_editor->applyDocument(m_result);

        // Should be able to edit again
        ok = m_editor->beginInlineEdit(EditTarget::Name, 1);
        QVERIFY(ok);
        QVERIFY(m_editor->isEditing());

        // Cancel again
        m_editor->cancelInlineEdit();
        QVERIFY(!m_editor->isEditing());
    }

    // ── Test: commit inline edit then re-edit same line ──
    void testCommitThenReEdit() {
        m_editor->applyDocument(m_result);
        m_editor->scintilla()->setCursorPosition(1, 0);

        // Begin value edit
        bool ok = m_editor->beginInlineEdit(EditTarget::Value, 1);
        QVERIFY(ok);
        QVERIFY(m_editor->isEditing());

        // Simulate Enter key → commit (via signal spy)
        QSignalSpy spy(m_editor, &RcxEditor::inlineEditCommitted);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(m_editor->scintilla(), &enter);

        // Should have emitted commit signal and exited edit mode
        QCOMPARE(spy.count(), 1);
        QVERIFY(!m_editor->isEditing());

        // Re-apply document (simulates refresh)
        m_editor->applyDocument(m_result);

        // Must be able to edit the same line again
        ok = m_editor->beginInlineEdit(EditTarget::Value, 1);
        QVERIFY(ok);
        QVERIFY(m_editor->isEditing());

        m_editor->cancelInlineEdit();
    }

    // ── Test: mouse click during edit commits it ──
    void testMouseClickCommitsEdit() {
        m_editor->applyDocument(m_result);

        bool ok = m_editor->beginInlineEdit(EditTarget::Name, 1);
        QVERIFY(ok);
        QVERIFY(m_editor->isEditing());

        // Simulate mouse click on viewport — should commit (save), not cancel
        QSignalSpy commitSpy(m_editor, &RcxEditor::inlineEditCommitted);
        QSignalSpy cancelSpy(m_editor, &RcxEditor::inlineEditCancelled);
        QMouseEvent click(QEvent::MouseButtonPress, QPointF(10, 10),
                          QPointF(10, 10), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(m_editor->scintilla()->viewport(), &click);

        QVERIFY(!m_editor->isEditing());
        QCOMPARE(commitSpy.count(), 1);
        QCOMPARE(cancelSpy.count(), 0);
    }

    // ── Test: FocusOut during edit commits it ──
    void testFocusOutCommitsEdit() {
        m_editor->applyDocument(m_result);

        // Give focus to the scintilla widget first
        m_editor->scintilla()->setFocus();
        QApplication::processEvents();

        bool ok = m_editor->beginInlineEdit(EditTarget::Name, 1);
        QVERIFY(ok);
        QVERIFY(m_editor->isEditing());

        QSignalSpy commitSpy(m_editor, &RcxEditor::inlineEditCommitted);
        QSignalSpy cancelSpy(m_editor, &RcxEditor::inlineEditCancelled);

        // Create a dummy widget and transfer focus to it (triggers real FocusOut)
        QWidget dummy;
        dummy.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dummy));
        dummy.setFocus();
        QApplication::processEvents();  // process focus change + deferred timer

        QVERIFY(!m_editor->isEditing());
        QCOMPARE(commitSpy.count(), 1);
        QCOMPARE(cancelSpy.count(), 0);

        // Restore focus to editor for subsequent tests
        m_editor->scintilla()->setFocus();
        QApplication::processEvents();
    }

    // ── Test: type edit begins and can be cancelled ──
    void testTypeEditCancel() {
        m_editor->applyDocument(m_result);

        // Begin type edit on a field line
        bool ok = m_editor->beginInlineEdit(EditTarget::Type, 1);
        QVERIFY(ok);
        QVERIFY(m_editor->isEditing());

        // Process deferred events (showTypeAutocomplete is deferred via QTimer)
        QApplication::processEvents();

        // First Escape closes autocomplete popup (if active) or cancels edit
        QKeyEvent esc1(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(m_editor->scintilla(), &esc1);

        // If autocomplete was open, first Esc only closed popup; need second Esc
        if (m_editor->isEditing()) {
            QKeyEvent esc2(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(m_editor->scintilla(), &esc2);
        }
        QVERIFY(!m_editor->isEditing());
    }

    // ── Test: edit on header line (Name is valid, Type/Value invalid) ──
    void testHeaderLineEdit() {
        m_editor->applyDocument(m_result);

        // Line 0 should be the struct header
        const LineMeta* lm = m_editor->metaForLine(0);
        QVERIFY(lm);
        QCOMPARE(lm->lineKind, LineKind::Header);

        // Type edit on header should fail (no type span)
        bool ok = m_editor->beginInlineEdit(EditTarget::Type, 0);
        QVERIFY(!ok);
        QVERIFY(!m_editor->isEditing());

        // Name edit on header should succeed (dynamic span)
        ok = m_editor->beginInlineEdit(EditTarget::Name, 0);
        QVERIFY(ok);
        QVERIFY(m_editor->isEditing());
        m_editor->cancelInlineEdit();
    }

    // ── Test: footer line rejects all edits ──
    void testFooterLineEdit() {
        m_editor->applyDocument(m_result);

        // Find the footer line
        int footerLine = -1;
        for (int i = 0; i < m_result.meta.size(); i++) {
            if (m_result.meta[i].lineKind == LineKind::Footer) {
                footerLine = i;
                break;
            }
        }
        QVERIFY(footerLine >= 0);

        QVERIFY(!m_editor->beginInlineEdit(EditTarget::Type, footerLine));
        QVERIFY(!m_editor->beginInlineEdit(EditTarget::Name, footerLine));
        QVERIFY(!m_editor->beginInlineEdit(EditTarget::Value, footerLine));
        QVERIFY(!m_editor->isEditing());
    }

    // ── Test: showTypeAutocomplete populates list (check via SCI_AUTOCACTIVE) ──
    void testTypeAutocompleteShows() {
        m_editor->applyDocument(m_result);

        bool ok = m_editor->beginInlineEdit(EditTarget::Type, 1);
        QVERIFY(ok);

        // Process deferred timer (autocomplete is deferred)
        QApplication::processEvents();

        // Check if the user list is active
        long active = m_editor->scintilla()->SendScintilla(
            QsciScintillaBase::SCI_AUTOCACTIVE);
        QVERIFY2(active != 0, "Autocomplete list should be active after type edit begins");

        // Cancel
        m_editor->cancelInlineEdit();
        m_editor->applyDocument(m_result);
    }

    // ── Test: parseValue accepts space-separated hex bytes ──
    void testParseValueHexWithSpaces() {
        bool ok;

        // Hex8 with spaces (single byte, but test the .remove(' '))
        QByteArray b = fmt::parseValue(NodeKind::Hex8, "4D", &ok);
        QVERIFY(ok);
        QCOMPARE((uint8_t)b[0], (uint8_t)0x4D);

        // Hex32 with space-separated bytes (raw byte order, no endian conversion)
        b = fmt::parseValue(NodeKind::Hex32, "DE AD BE EF", &ok);
        QVERIFY(ok);
        QCOMPARE(b.size(), 4);
        QCOMPARE((uint8_t)b[0], (uint8_t)0xDE);
        QCOMPARE((uint8_t)b[1], (uint8_t)0xAD);
        QCOMPARE((uint8_t)b[2], (uint8_t)0xBE);
        QCOMPARE((uint8_t)b[3], (uint8_t)0xEF);

        // Hex64 with space-separated bytes
        b = fmt::parseValue(NodeKind::Hex64, "4D 5A 90 00 00 00 00 00", &ok);
        QVERIFY(ok);
        QCOMPARE(b.size(), 8);
        QCOMPARE((uint8_t)b[0], (uint8_t)0x4D);
        QCOMPARE((uint8_t)b[1], (uint8_t)0x5A);
        QCOMPARE((uint8_t)b[7], (uint8_t)0x00);

        // Hex64 continuous (should still work)
        b = fmt::parseValue(NodeKind::Hex64, "4D5A900000000000", &ok);
        QVERIFY(ok);
        QCOMPARE((uint8_t)b[0], (uint8_t)0x4D);
        QCOMPARE((uint8_t)b[1], (uint8_t)0x5A);

        // Hex64 with 0x prefix and spaces
        b = fmt::parseValue(NodeKind::Hex64, "0x4D 5A 90 00 00 00 00 00", &ok);
        QVERIFY(ok);
    }

    // ── Test: type autocomplete accepts typed input and commits ──
    void testTypeAutocompleteTypingAndCommit() {
        m_editor->applyDocument(m_result);

        bool ok = m_editor->beginInlineEdit(EditTarget::Type, 1);
        QVERIFY(ok);

        // Process deferred autocomplete
        QApplication::processEvents();

        // Verify autocomplete is active
        long active = m_editor->scintilla()->SendScintilla(
            QsciScintillaBase::SCI_AUTOCACTIVE);
        QVERIFY2(active != 0, "Autocomplete should be active");

        // Simulate typing 'i' — filters to typeName entries starting with 'i'
        QKeyEvent keyI(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
        QApplication::sendEvent(m_editor->scintilla(), &keyI);

        // Still editing
        QVERIFY(m_editor->isEditing());

        // Simulate Enter to select from autocomplete (handled synchronously)
        QSignalSpy spy(m_editor, &RcxEditor::inlineEditCommitted);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(m_editor->scintilla(), &enter);

        // Should have committed immediately (no deferred timer for type edits)
        QCOMPARE(spy.count(), 1);
        QVERIFY(!m_editor->isEditing());

        // The committed text should be a valid typeName starting with 'i'
        QList<QVariant> args = spy.first();
        QString committedText = args.at(3).toString();
        QVERIFY2(committedText.startsWith('i'),
                 qPrintable("Expected typeName starting with 'i', got: " + committedText));

        m_editor->applyDocument(m_result);
    }

    // ── Test: type edit click-away commits original (no change) ──
    void testTypeEditClickAwayNoChange() {
        m_editor->applyDocument(m_result);

        bool ok = m_editor->beginInlineEdit(EditTarget::Type, 1);
        QVERIFY(ok);

        // Process deferred autocomplete
        QApplication::processEvents();

        // Click away on viewport — should commit (not cancel)
        QSignalSpy commitSpy(m_editor, &RcxEditor::inlineEditCommitted);
        QMouseEvent click(QEvent::MouseButtonPress, QPointF(10, 10),
                          QPointF(10, 10), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(m_editor->scintilla()->viewport(), &click);

        QVERIFY(!m_editor->isEditing());
        QCOMPARE(commitSpy.count(), 1);

        // The committed text should be the original typeName (no change)
        QList<QVariant> args = commitSpy.first();
        QString committedText = args.at(3).toString();
        QVERIFY2(committedText == "uint16_t",
                 qPrintable("Expected 'uint16_t', got: " + committedText));

        m_editor->applyDocument(m_result);
    }

    // ── Test: column span hit-testing for cursor shape ──
    void testColumnSpanHitTest() {
        m_editor->applyDocument(m_result);

        // Line 1 is a field line (UInt16), verify spans are valid
        const LineMeta* lm = m_editor->metaForLine(1);
        QVERIFY(lm);
        QCOMPARE(lm->lineKind, LineKind::Field);

        // Type span should be valid for field lines
        ColumnSpan ts = RcxEditor::typeSpan(*lm);
        QVERIFY(ts.valid);
        QVERIFY(ts.start < ts.end);

        // Name span should be valid for field lines
        ColumnSpan ns = RcxEditor::nameSpan(*lm);
        QVERIFY(ns.valid);
        QVERIFY(ns.start < ns.end);

        // Value span should be valid for field lines
        QString lineText;
        int len = (int)m_editor->scintilla()->SendScintilla(
            QsciScintillaBase::SCI_LINELENGTH, (unsigned long)1);
        QVERIFY(len > 0);
        ColumnSpan vs = RcxEditor::valueSpan(*lm, len);
        QVERIFY(vs.valid);
        QVERIFY(vs.start < vs.end);

        // Footer line should have no valid type/name spans
        int footerLine = -1;
        for (int i = 0; i < m_result.meta.size(); i++) {
            if (m_result.meta[i].lineKind == LineKind::Footer) {
                footerLine = i;
                break;
            }
        }
        QVERIFY(footerLine >= 0);
        const LineMeta* flm = m_editor->metaForLine(footerLine);
        QVERIFY(flm);
        ColumnSpan fts = RcxEditor::typeSpan(*flm);
        QVERIFY(!fts.valid);
        ColumnSpan fns = RcxEditor::nameSpan(*flm);
        QVERIFY(!fns.valid);
        ColumnSpan fvs = RcxEditor::valueSpan(*flm, 10);
        QVERIFY(!fvs.valid);
    }

    // ── Test: selectedNodeIndices ──
    void testSelectedNodeIndices() {
        m_editor->applyDocument(m_result);

        // Put cursor on first field line
        m_editor->scintilla()->setCursorPosition(1, 0);
        QSet<int> sel = m_editor->selectedNodeIndices();
        QCOMPARE(sel.size(), 1);

        // The node index should match the first field
        const LineMeta* lm = m_editor->metaForLine(1);
        QVERIFY(lm);
        QVERIFY(sel.contains(lm->nodeIdx));
    }
};

QTEST_MAIN(TestEditor)
#include "test_editor.moc"
