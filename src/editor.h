#pragma once
#include "core.h"
#include <QWidget>
#include <QSet>
#include <QPoint>

class QsciScintilla;
class QsciLexerCPP;

namespace rcx {

class RcxEditor : public QWidget {
    Q_OBJECT
public:
    explicit RcxEditor(QWidget* parent = nullptr);

    void applyDocument(const ComposeResult& result);

    ViewState saveViewState() const;
    void restoreViewState(const ViewState& vs);

    QsciScintilla* scintilla() const { return m_sci; }
    const LineMeta* metaForLine(int line) const;
    int currentNodeIndex() const;

    // ── Column span computation ──
    static ColumnSpan typeSpan(const LineMeta& lm);
    static ColumnSpan nameSpan(const LineMeta& lm);
    static ColumnSpan valueSpan(const LineMeta& lm, int lineLength);

    // ── Multi-selection ──
    QSet<int> selectedNodeIndices() const;

    // ── Inline editing ──
    bool isEditing() const { return m_editState.active; }
    bool beginInlineEdit(EditTarget target, int line = -1);
    void cancelInlineEdit();

    void applySelectionOverlay(const QSet<uint64_t>& selIds);

signals:
    void marginClicked(int margin, int line, Qt::KeyboardModifiers mods);
    void contextMenuRequested(int line, int nodeIdx, int subLine, QPoint globalPos);
    void nodeClicked(int line, uint64_t nodeId, Qt::KeyboardModifiers mods);
    void inlineEditCommitted(int nodeIdx, int subLine,
                             EditTarget target, const QString& text);
    void inlineEditCancelled();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QsciScintilla*    m_sci    = nullptr;
    QsciLexerCPP*     m_lexer  = nullptr;
    QVector<LineMeta> m_meta;

    int m_marginStyleBase = -1;
    int m_hintLine = -1;

    // ── Hover cursor + highlight ──
    QPoint m_lastHoverPos;
    bool   m_hoverInside = false;
    bool   m_cursorOverridden = false;
    uint64_t m_hoveredNodeId = 0;
    QSet<uint64_t> m_currentSelIds;

    // ── Drag selection ──
    bool m_dragging = false;
    int  m_dragLastLine = -1;

    // ── Inline edit state ──
    struct InlineEditState {
        bool       active    = false;
        int        line      = -1;
        int        nodeIdx   = -1;
        int        subLine   = 0;
        EditTarget target    = EditTarget::Name;
        int        spanStart = 0;
        int        linelenAfterReplace = 0;
        QString    original;
    };
    InlineEditState m_editState;

    void setupScintilla();
    void setupLexer();
    void setupMargins();
    void setupFolding();
    void setupMarkers();
    void allocateMarginStyles();

    void applyMarginText(const QVector<LineMeta>& meta);
    void applyMarkers(const QVector<LineMeta>& meta);
    void applyFoldLevels(const QVector<LineMeta>& meta);
    void applyHexDimming(const QVector<LineMeta>& meta);

    void commitInlineEdit();
    bool handleNormalKey(QKeyEvent* ke);
    bool handleEditKey(QKeyEvent* ke);
    void showTypeAutocomplete();
    void updateEditableUnderline(int line);
    void applyHoverCursor();
    void applyHoverHighlight();

    // ── Refactored helpers ──
    struct EndEditInfo { int nodeIdx; int subLine; EditTarget target; };
    EndEditInfo endInlineEdit();

    struct NormalizedSpan { int start = 0; int end = 0; bool valid = false; };
    NormalizedSpan normalizeSpan(const ColumnSpan& raw, const QString& lineText,
                                 EditTarget target, bool skipPrefixes) const;
};

} // namespace rcx
