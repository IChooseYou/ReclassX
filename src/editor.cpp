#include "editor.h"
#include <Qsci/qsciscintilla.h>
#include <Qsci/qsciscintillabase.h>
#include <Qsci/qscilexercpp.h>
#include <QVBoxLayout>
#include <QFont>
#include <QColor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QTimer>
#include <QCursor>
#include <QApplication>

namespace rcx {

// ── Theme constants ──
static const QColor kBgText("#1e1e1e");
static const QColor kBgMargin("#252526");
static const QColor kFgMargin("#858585");
static const QColor kFgMarginDim("#505050");

static constexpr int IND_EDITABLE  = 8;
static constexpr int IND_HEX_DIM  = 9;

static QString g_fontName = "Consolas";

static QFont editorFont() {
    QFont f(g_fontName, 12);
    f.setFixedPitch(true);
    return f;
}

RcxEditor::RcxEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_sci = new QsciScintilla(this);
    layout->addWidget(m_sci);

    setupScintilla();
    setupLexer();
    setupMargins();
    setupFolding();
    setupMarkers();
    allocateMarginStyles();

    m_sci->installEventFilter(this);
    m_sci->viewport()->installEventFilter(this);
    m_sci->viewport()->setMouseTracking(true);

    // Hover cursor is applied synchronously in eventFilter (no timer).

    connect(m_sci, &QsciScintilla::marginClicked,
            this, [this](int margin, int line, Qt::KeyboardModifiers mods) {
        emit marginClicked(margin, line, mods);
    });

    m_sci->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sci, &QWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        int line = m_sci->lineAt(pos);
        int nodeIdx = -1;
        int subLine = 0;
        if (line >= 0 && line < m_meta.size()) {
            nodeIdx = m_meta[line].nodeIdx;
            subLine = m_meta[line].subLine;
        }
        emit contextMenuRequested(line, nodeIdx, subLine, m_sci->mapToGlobal(pos));
    });

    connect(m_sci, &QsciScintilla::userListActivated,
            this, [this](int id, const QString& text) {
        if (id == 1 && m_editState.active && m_editState.target == EditTarget::Type) {
            auto info = endInlineEdit();
            emit inlineEditCommitted(info.nodeIdx, info.subLine, info.target, text);
        }
    });

    connect(m_sci, &QsciScintilla::cursorPositionChanged,
            this, [this](int line, int /*col*/) { updateEditableIndicators(line); });

    connect(m_sci, &QsciScintilla::textChanged, this, [this]() {
        if (!m_editState.active) return;
        if (m_editState.target == EditTarget::Value)
            validateEditLive();
    });
}

void RcxEditor::setupScintilla() {
    m_sci->setFont(editorFont());

    m_sci->setReadOnly(true);
    m_sci->setWrapMode(QsciScintilla::WrapNone);
    m_sci->setCaretLineVisible(false);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETCARETWIDTH, 0);

    // Arrow cursor by default — not the I-beam (this is a structured viewer, not a text editor)
    m_sci->viewport()->setCursor(Qt::ArrowCursor);

    m_sci->setPaper(kBgText);
    m_sci->setColor(QColor("#d4d4d4"));

    m_sci->setTabWidth(2);
    m_sci->setIndentationsUseTabs(false);

    // Caret color for dark theme
    m_sci->setCaretForegroundColor(QColor("#d4d4d4"));

    // Line spacing for readability
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETEXTRAASCENT, (long)2);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETEXTRADESCENT, (long)2);

    // Disable native selection rendering — we use markers for selection
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETSELFORE, (long)0, (long)0);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETSELBACK, (long)0, (long)0);

    // Editable-field link-style indicator (colored text)
    m_sci->SendScintilla(QsciScintillaBase::SCI_INDICSETSTYLE,
                         IND_EDITABLE, 17 /*INDIC_TEXTFORE*/);
    m_sci->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE,
                         IND_EDITABLE, QColor("#569cd6"));

    // Hex/Padding node dim indicator — overrides text color to gray
    m_sci->SendScintilla(QsciScintillaBase::SCI_INDICSETSTYLE,
                         IND_HEX_DIM, 17 /*INDIC_TEXTFORE*/);
    m_sci->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE,
                         IND_HEX_DIM, QColor("#505050"));

}

void RcxEditor::setupLexer() {
    m_lexer = new QsciLexerCPP(m_sci);
    QFont font = editorFont();
    m_lexer->setFont(font);

    // Dark theme colors
    m_lexer->setColor(QColor("#569cd6"), QsciLexerCPP::Keyword);
    m_lexer->setColor(QColor("#4ec9b0"), QsciLexerCPP::KeywordSet2);
    m_lexer->setColor(QColor("#b5cea8"), QsciLexerCPP::Number);
    m_lexer->setColor(QColor("#ce9178"), QsciLexerCPP::DoubleQuotedString);
    m_lexer->setColor(QColor("#ce9178"), QsciLexerCPP::SingleQuotedString);
    m_lexer->setColor(QColor("#6a9955"), QsciLexerCPP::Comment);
    m_lexer->setColor(QColor("#6a9955"), QsciLexerCPP::CommentLine);
    m_lexer->setColor(QColor("#6a9955"), QsciLexerCPP::CommentDoc);
    m_lexer->setColor(QColor("#d4d4d4"), QsciLexerCPP::Default);
    m_lexer->setColor(QColor("#d4d4d4"), QsciLexerCPP::Identifier);
    m_lexer->setColor(QColor("#c586c0"), QsciLexerCPP::PreProcessor);
    m_lexer->setColor(QColor("#d4d4d4"), QsciLexerCPP::Operator);

    // Dark background for all styles
    for (int i = 0; i <= 127; i++) {
        m_lexer->setPaper(kBgText, i);
        m_lexer->setFont(font, i);
    }

    m_sci->setLexer(m_lexer);
    m_sci->setBraceMatching(QsciScintilla::SloppyBraceMatch);

    // Add type names to keyword set 2 → teal coloring (distinct from identifiers)
    QByteArray kw2 = allTypeNamesForUI(/*stripBrackets=*/true).join(' ').toLatin1();
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETKEYWORDS,
                         (uintptr_t)1, kw2.constData());
}

void RcxEditor::setupMargins() {
    m_sci->setMarginsFont(editorFont());

    // Margin 0: Offset text
    m_sci->setMarginType(0, QsciScintilla::TextMarginRightJustified);
    m_sci->setMarginWidth(0, "  +0x00000000  ");
    m_sci->setMarginsBackgroundColor(kBgMargin);
    m_sci->setMarginsForegroundColor(kFgMarginDim);
    m_sci->setMarginSensitivity(0, true);

    // Margin 1: hidden (fold chevrons moved to text column)
    m_sci->setMarginWidth(1, 0);
}

void RcxEditor::setupFolding() {
    // Hide fold margin (fold indicators are text-based now)
    m_sci->setMarginWidth(2, 0);
    m_sci->setFoldMarginColors(kBgMargin, kBgMargin);

    // Fold indicators are now text in the line content (kFoldCol prefix),
    // so no Scintilla markers needed for fold state.

    // Keep Scintilla fold markers invisible (fold levels still used for click detection)
    for (int i = 25; i <= 31; i++)
        m_sci->markerDefine(QsciScintilla::Invisible, i);

    // Disable automatic fold toggle — we handle collapse at model level
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETAUTOMATICFOLD,
                         (unsigned long)0);

    // Disable lexer-driven folding — we set fold levels manually
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETPROPERTY,
                         (const char*)"fold", (const char*)"0");
}

void RcxEditor::setupMarkers() {
    // M_CONT (0): continuation line (metadata only, no visual)
    m_sci->markerDefine(QsciScintilla::Invisible, M_CONT);

    // M_PAD (1): padding line (metadata only, no visual)
    m_sci->markerDefine(QsciScintilla::Invisible, M_PAD);

    // M_PTR0 (2): right triangle (red)
    m_sci->markerDefine(QsciScintilla::RightTriangle, M_PTR0);
    m_sci->setMarkerBackgroundColor(QColor("#f44747"), M_PTR0);
    m_sci->setMarkerForegroundColor(QColor("#f44747"), M_PTR0);

    // M_CYCLE (3): arrows (orange)
    m_sci->markerDefine(QsciScintilla::ThreeRightArrows, M_CYCLE);
    m_sci->setMarkerBackgroundColor(QColor("#e5a00d"), M_CYCLE);
    m_sci->setMarkerForegroundColor(QColor("#e5a00d"), M_CYCLE);

    // M_ERR (4): background (dark red - brightened for visibility)
    m_sci->markerDefine(QsciScintilla::Background, M_ERR);
    m_sci->setMarkerBackgroundColor(QColor("#7a2e2e"), M_ERR);
    m_sci->setMarkerForegroundColor(QColor("#ffffff"), M_ERR);

    // M_STRUCT_BG (5): struct header/footer (matches regular bg, may remove later)
    m_sci->markerDefine(QsciScintilla::Background, M_STRUCT_BG);
    m_sci->setMarkerBackgroundColor(QColor("#1e1e1e"), M_STRUCT_BG);
    m_sci->setMarkerForegroundColor(QColor("#d4d4d4"), M_STRUCT_BG);

    // M_HOVER (6): full-row hover highlight
    m_sci->markerDefine(QsciScintilla::Background, M_HOVER);
    m_sci->setMarkerBackgroundColor(QColor(43, 43, 43), M_HOVER);

    // M_SELECTED (7): full-row selection highlight (higher = wins over hover)
    m_sci->markerDefine(QsciScintilla::Background, M_SELECTED);
    m_sci->setMarkerBackgroundColor(QColor(53, 53, 53), M_SELECTED);
}

void RcxEditor::allocateMarginStyles() {
    static constexpr int MSTYLE_NORMAL = 0;
    static constexpr int MSTYLE_CONT   = 1;

    long base = m_sci->SendScintilla(QsciScintillaBase::SCI_ALLOCATEEXTENDEDSTYLES, (long)2);
    m_marginStyleBase = (int)base;
    m_sci->SendScintilla(QsciScintillaBase::SCI_MARGINSETSTYLEOFFSET, base);

    const long bgrMargin = 0x262525; // BGR for #252526
    QByteArray fontName = editorFont().family().toUtf8();
    int fontSize = editorFont().pointSize();

    // Margin styles (dim gray text)
    for (int s = MSTYLE_NORMAL; s <= MSTYLE_CONT; s++) {
        unsigned long abs = (unsigned long)(base + s);
        m_sci->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, abs, (long)0x505050);
        m_sci->SendScintilla(QsciScintillaBase::SCI_STYLESETBACK, abs, bgrMargin);
        m_sci->SendScintilla(QsciScintillaBase::SCI_STYLESETFONT,
                             (uintptr_t)abs, fontName.constData());
        m_sci->SendScintilla(QsciScintillaBase::SCI_STYLESETSIZE, abs, (long)fontSize);
    }
}

void RcxEditor::applyDocument(const ComposeResult& result) {
    // Silently deactivate inline edit (no signal — refresh is already happening)
    if (m_editState.active)
        endInlineEdit();

    m_meta = result.meta;

    m_sci->setReadOnly(false);
    m_sci->setText(result.text);
    m_sci->setReadOnly(true);

    // Force full re-lex to fix stale syntax coloring after edits
    m_sci->SendScintilla(QsciScintillaBase::SCI_COLOURISE, (uintptr_t)0, (long)-1);

    applyMarginText(result.meta);
    applyMarkers(result.meta);
    applyFoldLevels(result.meta);
    applyHexDimming(result.meta);

    // Reset hint line - applySelectionOverlay will repaint indicators
    m_hintLine = -1;
}

void RcxEditor::applyMarginText(const QVector<LineMeta>& meta) {
    // Clear all margin text
    m_sci->clearMarginText(-1);

    for (int i = 0; i < meta.size(); i++) {
        const auto& lm = meta[i];
        if (lm.offsetText.isEmpty()) continue;

        QByteArray text = lm.offsetText.toUtf8();
        m_sci->SendScintilla(QsciScintillaBase::SCI_MARGINSETTEXT,
                             (uintptr_t)i, text.constData());
        QByteArray styles(text.size(), '\0');  // style 0 = dim
        m_sci->SendScintilla(QsciScintillaBase::SCI_MARGINSETSTYLES,
                             (uintptr_t)i, styles.constData());
    }
}

void RcxEditor::applyMarkers(const QVector<LineMeta>& meta) {
    for (int m = M_CONT; m <= M_STRUCT_BG; m++) {
        m_sci->markerDeleteAll(m);
    }
    for (int i = 0; i < meta.size(); i++) {
        uint32_t mask = meta[i].markerMask;
        for (int m = M_CONT; m <= M_STRUCT_BG; m++) {
            if (mask & (1u << m)) {
                m_sci->markerAdd(i, m);
            }
        }
    }
}

void RcxEditor::applyFoldLevels(const QVector<LineMeta>& meta) {
    for (int i = 0; i < meta.size(); i++) {
        m_sci->SendScintilla(QsciScintillaBase::SCI_SETFOLDLEVEL,
                             (unsigned long)i, (long)meta[i].foldLevel);
    }
}

static inline void lineRangeNoEol(QsciScintilla* sci, int line, long& start, long& len) {
    start = sci->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, (unsigned long)line);
    long end = sci->SendScintilla(QsciScintillaBase::SCI_GETLINEENDPOSITION, (unsigned long)line);
    len = (end > start) ? (end - start) : 0;
}

// UTF-8 safe column-to-position conversion
static inline long posFromCol(QsciScintilla* sci, int line, int col) {
    return sci->SendScintilla(QsciScintillaBase::SCI_FINDCOLUMN,
                              (unsigned long)line, (long)col);
}

void RcxEditor::clearIndicatorLine(int indic, int line) {
    if (line < 0) return;
    long start, len;
    lineRangeNoEol(m_sci, line, start, len);
    if (len <= 0) return;
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, indic);
    m_sci->SendScintilla(QsciScintillaBase::SCI_INDICATORCLEARRANGE, start, len);
}

void RcxEditor::fillIndicatorCols(int indic, int line, int colA, int colB) {
    long a = posFromCol(m_sci, line, colA);
    long b = posFromCol(m_sci, line, colB);
    if (b > a) {
        m_sci->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, indic);
        m_sci->SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE, a, b - a);
    }
}

void RcxEditor::applyHexDimming(const QVector<LineMeta>& meta) {
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, IND_HEX_DIM);
    for (int i = 0; i < meta.size(); i++) {
        if (isHexPreview(meta[i].nodeKind)) {
            long pos, len; lineRangeNoEol(m_sci, i, pos, len);
            if (len > 0)
                m_sci->SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE, pos, len);
        }
    }
}

void RcxEditor::applySelectionOverlay(const QSet<uint64_t>& selIds) {
    m_currentSelIds = selIds;
    m_sci->markerDeleteAll(M_SELECTED);

    // Clear all editable indicators, then repaint for selected lines only
    long docLen = m_sci->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, IND_EDITABLE);
    m_sci->SendScintilla(QsciScintillaBase::SCI_INDICATORCLEARRANGE, (unsigned long)0, docLen);

    for (int i = 0; i < m_meta.size(); i++) {
        if (selIds.contains(m_meta[i].nodeId)) {
            m_sci->markerAdd(i, M_SELECTED);
            paintEditableSpans(i);
        }
    }

    // Reset hint line - updateEditableIndicators will handle cursor hints
    // on actual user navigation (not stale restored positions)
    m_hintLine = -1;

    applyHoverHighlight();
}

void RcxEditor::applyHoverHighlight() {
    m_sci->markerDeleteAll(M_HOVER);
    if (m_editState.active) return;
    if (!m_hoverInside) return;
    if (m_hoveredNodeId == 0) return;
    if (m_currentSelIds.contains(m_hoveredNodeId)) return;
    for (int i = 0; i < m_meta.size(); i++) {
        if (m_meta[i].nodeId == m_hoveredNodeId)
            m_sci->markerAdd(i, M_HOVER);
    }
}

ViewState RcxEditor::saveViewState() const {
    ViewState vs;
    vs.scrollLine = (int)m_sci->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
    int line, col;
    m_sci->getCursorPosition(&line, &col);
    vs.cursorLine = line;
    vs.cursorCol  = col;
    return vs;
}

void RcxEditor::restoreViewState(const ViewState& vs) {
    int maxLine = std::max(0, m_sci->lines() - 1);
    int line = std::clamp(vs.cursorLine, 0, maxLine);
    long pos = m_sci->SendScintilla(QsciScintillaBase::SCI_FINDCOLUMN,
                                    (unsigned long)line,
                                    (long)std::max(0, vs.cursorCol));
    m_sci->SendScintilla(QsciScintillaBase::SCI_GOTOPOS, (unsigned long)pos);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE,
                         (unsigned long)vs.scrollLine);
}

const LineMeta* RcxEditor::metaForLine(int line) const {
    if (line >= 0 && line < m_meta.size())
        return &m_meta[line];
    return nullptr;
}

int RcxEditor::currentNodeIndex() const {
    int line, col;
    m_sci->getCursorPosition(&line, &col);
    auto* lm = metaForLine(line);
    return lm ? lm->nodeIdx : -1;
}

// ── Column span computation ──

ColumnSpan RcxEditor::typeSpan(const LineMeta& lm)  { return typeSpanFor(lm); }
ColumnSpan RcxEditor::nameSpan(const LineMeta& lm)  { return nameSpanFor(lm); }
ColumnSpan RcxEditor::valueSpan(const LineMeta& lm, int lineLength) { return valueSpanFor(lm, lineLength); }

// ── Multi-selection ──

QSet<int> RcxEditor::selectedNodeIndices() const {
    int lineFrom, indexFrom, lineTo, indexTo;
    m_sci->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    if (lineFrom < 0) {
        int line, col;
        m_sci->getCursorPosition(&line, &col);
        auto* lm = metaForLine(line);
        return lm && lm->nodeIdx >= 0 ? QSet<int>{lm->nodeIdx} : QSet<int>{};
    }
    QSet<int> result;
    for (int line = lineFrom; line <= lineTo; line++) {
        auto* lm = metaForLine(line);
        if (lm && lm->nodeIdx >= 0) result.insert(lm->nodeIdx);
    }
    return result;
}

// ── Inline edit helpers ──

static QString getLineText(QsciScintilla* sci, int line) {
    int len = (int)sci->SendScintilla(QsciScintillaBase::SCI_LINELENGTH, (unsigned long)line);
    if (len <= 0) return {};
    QByteArray buf(len + 1, '\0');
    sci->SendScintilla(QsciScintillaBase::SCI_GETLINE, (unsigned long)line, (void*)buf.data());
    QString text = QString::fromUtf8(buf.data(), len);
    while (text.endsWith('\n') || text.endsWith('\r'))
        text.chop(1);
    return text;
}

// ── Shared inline-edit shutdown ──

RcxEditor::EndEditInfo RcxEditor::endInlineEdit() {
    // Clear edit comment and error marker before deactivating
    if (m_editState.target == EditTarget::Value) {
        setEditComment({});  // Clear to spaces
        m_sci->markerDelete(m_editState.line, M_ERR);
    }
    EndEditInfo info{m_editState.nodeIdx, m_editState.subLine, m_editState.target};
    m_editState.active = false;
    m_sci->setReadOnly(true);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETCARETWIDTH, 0);
    // Switch from I-beam to Arrow (keep override active to block Scintilla's cursor)
    if (m_cursorOverridden) {
        QApplication::changeOverrideCursor(Qt::ArrowCursor);
    } else {
        QApplication::setOverrideCursor(Qt::ArrowCursor);
        m_cursorOverridden = true;
    }
    // Disable selection rendering again
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETSELFORE, (long)0, (long)0);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETSELBACK, (long)0, (long)0);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, (long)1);
    m_sci->SendScintilla(QsciScintillaBase::SCI_EMPTYUNDOBUFFER);
    return info;
}

// ── Span helpers ──

static ColumnSpan headerNameSpan(const LineMeta& lm, const QString& lineText) {
    if (lm.lineKind != LineKind::Header) return {};
    int bracePos = lineText.lastIndexOf(QStringLiteral(" {"));
    if (bracePos <= 0) return {};
    int ind = kFoldCol + lm.depth * 3;
    int typeEnd = lineText.indexOf(' ', ind);
    if (typeEnd <= ind || typeEnd >= bracePos) return {};
    return {typeEnd + 1, bracePos, true};
}

RcxEditor::NormalizedSpan RcxEditor::normalizeSpan(
    const ColumnSpan& raw, const QString& lineText,
    EditTarget target, bool skipPrefixes) const
{
    if (!raw.valid) return {};
    int textLen = lineText.size();
    if (raw.start >= textLen) return {};

    int start = raw.start;
    int end   = qMin(raw.end, textLen);
    if (end <= start) return {};

    if (skipPrefixes && target == EditTarget::Value) {
        QString spanText = lineText.mid(start, end - start);
        int arrow = spanText.indexOf(QStringLiteral("->"));
        if (arrow >= 0) {
            int i = arrow + 2;
            while (i < spanText.size() && spanText[i].isSpace()) i++;
            start += i;
        } else {
            int eq = spanText.indexOf('=');
            if (eq >= 0 && eq <= 3) {
                int i = eq + 1;
                while (i < spanText.size() && spanText[i].isSpace()) i++;
                start += i;
            }
        }
        if (start >= end) return {};
    }

    QString inner = lineText.mid(start, end - start);
    int lead = 0;
    while (lead < inner.size() && inner[lead].isSpace()) lead++;
    int trail = inner.size();
    while (trail > lead && inner[trail - 1].isSpace()) trail--;
    if (trail <= lead) return {};

    return {start + lead, start + trail, true};
}

bool RcxEditor::resolvedSpanFor(int line, EditTarget t,
                                NormalizedSpan& out, QString* lineTextOut) const {
    const LineMeta* lm = metaForLine(line);
    if (!lm || lm->nodeIdx < 0) return false;

    QString lineText = getLineText(m_sci, line);
    int textLen = lineText.size();

    ColumnSpan s;
    switch (t) {
    case EditTarget::Type:  s = typeSpan(*lm); break;
    case EditTarget::Name:  s = nameSpan(*lm); break;
    case EditTarget::Value: s = valueSpan(*lm, textLen); break;
    }

    if (!s.valid && t == EditTarget::Name)
        s = headerNameSpan(*lm, lineText);

    out = normalizeSpan(s, lineText, t, /*skipPrefixes=*/true);
    if (lineTextOut) *lineTextOut = lineText;
    return out.valid;
}

// ── Point → line/col/nodeId resolution ──

RcxEditor::HitInfo RcxEditor::hitTest(const QPoint& vp) const {
    HitInfo h;

    // Try precise position first (works when cursor is over actual text)
    long pos = m_sci->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMPOINTCLOSE,
                                     (unsigned long)vp.x(), (long)vp.y());
    if (pos >= 0) {
        h.line = (int)m_sci->SendScintilla(
            QsciScintillaBase::SCI_LINEFROMPOSITION, (unsigned long)pos);
        h.col = (int)m_sci->SendScintilla(
            QsciScintillaBase::SCI_GETCOLUMN, (unsigned long)pos);
    } else {
        // Fallback: calculate line from Y coordinate (for empty space past text)
        int firstVisible = (int)m_sci->SendScintilla(
            QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
        int lineHeight = (int)m_sci->SendScintilla(
            QsciScintillaBase::SCI_TEXTHEIGHT, 0);
        if (lineHeight > 0)
            h.line = firstVisible + vp.y() / lineHeight;
    }

    if (h.line >= 0 && h.line < m_meta.size()) {
        h.nodeId = m_meta[h.line].nodeId;
        h.inFoldCol = (h.col >= 0 && h.col < kFoldCol && m_meta[h.line].foldHead);
    }
    return h;
}

// ── Double-click hit test ──

static bool hitTestTarget(QsciScintilla* sci,
                          const QVector<LineMeta>& meta,
                          const QPoint& viewportPos,
                          int& outLine, EditTarget& outTarget)
{
    long pos = sci->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMPOINTCLOSE,
                                  (unsigned long)viewportPos.x(), (long)viewportPos.y());
    if (pos < 0) return false;
    int line = (int)sci->SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION,
                                       (unsigned long)pos);
    int col  = (int)sci->SendScintilla(QsciScintillaBase::SCI_GETCOLUMN,
                                       (unsigned long)pos);
    if (line < 0 || line >= meta.size()) return false;

    QString lineText = getLineText(sci, line);
    int textLen = lineText.size();

    const LineMeta& lm = meta[line];
    ColumnSpan ts = RcxEditor::typeSpan(lm);
    ColumnSpan ns = RcxEditor::nameSpan(lm);
    ColumnSpan vs = RcxEditor::valueSpan(lm, textLen);

    if (!ns.valid)
        ns = headerNameSpan(lm, lineText);

    auto inSpan = [&](const ColumnSpan& s) {
        return s.valid && col >= s.start && col < s.end;
    };

    if (inSpan(ts))      outTarget = EditTarget::Type;
    else if (inSpan(ns)) outTarget = EditTarget::Name;
    else if (inSpan(vs)) outTarget = EditTarget::Value;
    else return false;

    outLine = line;
    return true;
}

// ── Event filter ──

bool RcxEditor::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_sci && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        return m_editState.active ? handleEditKey(ke) : handleNormalKey(ke);
    }
    if (obj == m_sci->viewport() && event->type() == QEvent::MouseButtonPress
        && m_editState.active) {
        auto* me = static_cast<QMouseEvent*>(event);
        auto h = hitTest(me->pos());
        bool insideEdit = false;
        if (h.line == m_editState.line) {
            int editEnd = editEndCol();
            insideEdit = (h.col >= m_editState.spanStart && h.col <= editEnd);
        }
        if (insideEdit)
            return false;   // inside edit span: let Scintilla position cursor
        commitInlineEdit();
        m_currentSelIds.clear();   // stale — normal handler will re-establish
        // Fall through to normal click handler below
    }
    // Single-click on fold column (" - " / " + ") toggles fold
    // Other left-clicks emit nodeClicked for selection
    if (obj == m_sci->viewport() && !m_editState.active
        && event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            // Sync hover to click position (prevents hover/selection desync)
            m_lastHoverPos = me->pos();
            m_hoverInside = true;
            auto h = hitTest(me->pos());
            uint64_t newHoverId = (h.line >= 0) ? h.nodeId : 0;
            if (newHoverId != m_hoveredNodeId) {
                m_hoveredNodeId = newHoverId;
                applyHoverHighlight();
            }

            if (h.inFoldCol) {
                emit marginClicked(0, h.line, me->modifiers());
                return true;
            }
            if (h.nodeId != 0) {
                bool alreadySelected = m_currentSelIds.contains(h.nodeId);
                bool plain = !(me->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));

                // Single-click on editable token of already-selected node → edit
                if (alreadySelected && plain) {
                    int tLine; EditTarget t;
                    if (hitTestTarget(m_sci, m_meta, me->pos(), tLine, t)) {
                        m_pendingClickNodeId = 0;
                        return beginInlineEdit(t, tLine);
                    }
                }

                m_dragging = true;
                m_dragStarted = false;  // require threshold before extending
                m_dragStartPos = me->pos();
                m_dragLastLine = h.line;
                m_dragInitMods = me->modifiers();

                bool multi = m_currentSelIds.size() > 1;

                if (alreadySelected && multi && plain) {
                    // Defer: might be start of double-click-to-edit
                    m_pendingClickNodeId = h.nodeId;
                    m_pendingClickLine = h.line;
                    m_pendingClickMods = me->modifiers();
                } else {
                    emit nodeClicked(h.line, h.nodeId, me->modifiers());
                    m_pendingClickNodeId = 0;
                }
            }
        }
    }
    // Drag-select: extend selection as mouse moves with button held
    // Requires minimum drag distance to prevent accidental micro-drag selection
    if (obj == m_sci->viewport() && !m_editState.active
        && event->type() == QEvent::MouseMove && m_dragging) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->buttons() & Qt::LeftButton) {
            // Check drag threshold (8 pixels) before starting drag-selection
            if (!m_dragStarted) {
                int dy = me->pos().y() - m_dragStartPos.y();
                if (qAbs(dy) < 8)
                    return false;  // not yet a drag, let Scintilla handle
                m_dragStarted = true;
            }

            // Flush deferred click before extending drag
            if (m_pendingClickNodeId != 0) {
                emit nodeClicked(m_pendingClickLine, m_pendingClickNodeId,
                                 m_pendingClickMods);
                m_pendingClickNodeId = 0;
            }
            auto h = hitTest(me->pos());
            if (h.line >= 0 && h.line != m_dragLastLine && h.nodeId != 0) {
                emit nodeClicked(h.line, h.nodeId, m_dragInitMods | Qt::ShiftModifier);
                m_dragLastLine = h.line;
            }
        } else {
            m_dragging = false;
            m_dragStarted = false;
        }
    }
    if (obj == m_sci->viewport() && event->type() == QEvent::MouseButtonRelease) {
        m_dragging = false;
        m_dragStarted = false;
        if (m_pendingClickNodeId != 0) {
            emit nodeClicked(m_pendingClickLine, m_pendingClickNodeId,
                             m_pendingClickMods);
            m_pendingClickNodeId = 0;
        }
    }
    // Block double/triple-click during edit mode (prevents word/line selection)
    if (obj == m_sci->viewport() && m_editState.active
        && event->type() == QEvent::MouseButtonDblClick) {
        return true;
    }
    if (obj == m_sci->viewport() && !m_editState.active
        && event->type() == QEvent::MouseButtonDblClick) {
        auto* me = static_cast<QMouseEvent*>(event);
        int line; EditTarget t;
        if (hitTestTarget(m_sci, m_meta, me->pos(), line, t)) {
            m_pendingClickNodeId = 0;   // cancel deferred selection change
            return beginInlineEdit(t, line);
        }
    }
    if (obj == m_sci && event->type() == QEvent::FocusOut) {
        auto* fe = static_cast<QFocusEvent*>(event);
        // Commit active edit on focus loss (click-away = save)
        // Deferred so autocomplete popup has time to register as active
        if (m_editState.active && fe->reason() != Qt::PopupFocusReason) {
            QTimer::singleShot(0, this, [this]() {
                if (m_editState.active && !m_sci->hasFocus()
                    && !m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCACTIVE))
                    commitInlineEdit();
            });
        }
        // Clear editable indicators when editor loses focus
        clearIndicatorLine(IND_EDITABLE, m_hintLine);
        m_hintLine = -1;
    }
    if (obj == m_sci && event->type() == QEvent::FocusIn) {
        int line, col;
        m_sci->getCursorPosition(&line, &col);
        updateEditableIndicators(line);
    }
    if (obj == m_sci->viewport() && !m_editState.active) {
        if (event->type() == QEvent::MouseMove) {
            m_lastHoverPos = static_cast<QMouseEvent*>(event)->pos();
            m_hoverInside = true;
        } else if (event->type() == QEvent::Leave) {
            m_hoverInside = false;
            m_hoveredNodeId = 0;
            applyHoverHighlight();
        } else if (event->type() == QEvent::Wheel) {
            m_lastHoverPos = m_sci->viewport()->mapFromGlobal(QCursor::pos());
            m_hoverInside = m_sci->viewport()->rect().contains(m_lastHoverPos);
        }
        // Resolve hovered nodeId on move/wheel
        if (event->type() == QEvent::MouseMove
         || event->type() == QEvent::Wheel) {
            auto h = hitTest(m_lastHoverPos);
            uint64_t newHoverId = (m_hoverInside && h.line >= 0) ? h.nodeId : 0;
            if (newHoverId != m_hoveredNodeId) {
                m_hoveredNodeId = newHoverId;
                applyHoverHighlight();
            }
        }
        if (event->type() == QEvent::MouseMove
         || event->type() == QEvent::Leave
         || event->type() == QEvent::Wheel)
            applyHoverCursor();
    }
    return QWidget::eventFilter(obj, event);
}

// ── Normal mode key handling ──

bool RcxEditor::handleNormalKey(QKeyEvent* ke) {
    switch (ke->key()) {
    case Qt::Key_F2:
        return beginInlineEdit(EditTarget::Name);
    case Qt::Key_T:
        if (ke->modifiers() == Qt::NoModifier)
            return beginInlineEdit(EditTarget::Type);
        return false;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return beginInlineEdit(EditTarget::Value);
    default:
        return false;
    }
}

// ── Edit mode key handling ──

bool RcxEditor::handleEditKey(QKeyEvent* ke) {
    bool autocActive = m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCACTIVE);

    switch (ke->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Tab:
        if (autocActive && m_editState.target == EditTarget::Type) {
            // Extract selected typeName directly from autocomplete
            QByteArray buf(256, '\0');
            m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCGETCURRENTTEXT,
                                 (unsigned long)256, (void*)buf.data());
            QString selectedType = QString::fromUtf8(buf.constData());
            m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCCANCEL);

            auto info = endInlineEdit();
            emit inlineEditCommitted(info.nodeIdx, info.subLine, EditTarget::Type, selectedType);
            return true;
        }
        commitInlineEdit();
        return true;
    case Qt::Key_Escape:
        if (autocActive) {
            m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCCANCEL);
            return true;  // close popup, stay in edit mode
        }
        cancelInlineEdit();
        return true;
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        if (autocActive) return false;  // let Scintilla navigate list
        return true;  // block line navigation
    case Qt::Key_Delete:
        return true;  // block to prevent eating trailing content
    case Qt::Key_Left:
    case Qt::Key_Backspace: {
        int line, col;
        m_sci->getCursorPosition(&line, &col);
        if (col <= m_editState.spanStart) return true;
        return false;
    }
    case Qt::Key_Right: {
        int line, col;
        m_sci->getCursorPosition(&line, &col);
        if (col >= editEndCol()) return true;  // block past end
        return false;
    }
    case Qt::Key_Home:
        m_sci->setCursorPosition(m_editState.line, m_editState.spanStart);
        return true;
    case Qt::Key_End:
        m_sci->setCursorPosition(m_editState.line, editEndCol());
        return true;
    default:
        return false;
    }
}

// ── Begin inline edit ──

bool RcxEditor::beginInlineEdit(EditTarget target, int line) {
    if (m_editState.active) return false;
    m_hoveredNodeId = 0;
    applyHoverHighlight();
    // Clear editable-token color hints (de-emphasize non-active tokens)
    clearIndicatorLine(IND_EDITABLE, m_hintLine);
    m_hintLine = -1;

    if (line >= 0) {
        m_sci->setCursorPosition(line, 0);
    }
    int col;
    m_sci->getCursorPosition(&line, &col);
    auto* lm = metaForLine(line);
    if (!lm || lm->nodeIdx < 0) return false;

    QString lineText;
    NormalizedSpan norm;
    if (!resolvedSpanFor(line, target, norm, &lineText)) return false;

    QString trimmed = lineText.mid(norm.start, norm.end - norm.start);

    m_editState.active = true;
    m_editState.line = line;
    m_editState.nodeIdx = lm->nodeIdx;
    m_editState.subLine = lm->subLine;
    m_editState.target = target;
    m_editState.spanStart = norm.start;
    m_editState.original = trimmed;
    m_editState.linelenAfterReplace = lineText.size();
    m_editState.editKind = lm->nodeKind;
    if ((lm->nodeKind == NodeKind::Vec2 || lm->nodeKind == NodeKind::Vec3 ||
         lm->nodeKind == NodeKind::Vec4) && lm->subLine > 0)
        m_editState.editKind = NodeKind::Float;

    // Disable Scintilla undo during inline edit
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, (long)0);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETCARETWIDTH, 1);
    m_sci->setReadOnly(false);
    // Switch to I-beam for editing
    if (m_cursorOverridden) {
        QApplication::changeOverrideCursor(Qt::IBeamCursor);
    } else {
        QApplication::setOverrideCursor(Qt::IBeamCursor);
        m_cursorOverridden = true;
    }

    // Re-enable selection rendering for inline edit
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETSELFORE, (long)0, (long)0);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETSELBACK, (long)1,
                         QColor("#264f78"));

    long lineStart = m_sci->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE,
                                           (unsigned long)line);
    long posStart = lineStart + m_editState.spanStart;
    long posEnd   = posStart + trimmed.toUtf8().size();
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETSEL, posEnd, posEnd);

    // Show initial edit hint in comment column
    if (target == EditTarget::Value)
        setEditComment(QStringLiteral("// Enter=Save Esc=Cancel"));

    if (target == EditTarget::Type)
        QTimer::singleShot(0, this, &RcxEditor::showTypeAutocomplete);

    return true;
}

int RcxEditor::editEndCol() const {
    QString lineText = getLineText(m_sci, m_editState.line);
    int delta = lineText.size() - m_editState.linelenAfterReplace;
    return m_editState.spanStart + m_editState.original.size() + delta;
}

// ── Commit inline edit ──

void RcxEditor::commitInlineEdit() {
    if (!m_editState.active) return;

    QString lineText = getLineText(m_sci, m_editState.line);
    int currentLen = lineText.size();
    int delta = currentLen - m_editState.linelenAfterReplace;
    int editedLen = m_editState.original.size() + delta;

    QString editedText;
    if (editedLen > 0)
        editedText = lineText.mid(m_editState.spanStart, editedLen).trimmed();

    auto info = endInlineEdit();
    emit inlineEditCommitted(info.nodeIdx, info.subLine, info.target, editedText);
}

// ── Cancel inline edit ──

void RcxEditor::cancelInlineEdit() {
    if (!m_editState.active) return;

    endInlineEdit();
    emit inlineEditCancelled();
}

// ── Type autocomplete ──

void RcxEditor::showTypeAutocomplete() {
    if (!m_editState.active || m_editState.target != EditTarget::Type)
        return;

    // Collapse selection to start — old type text stays visible
    long lineStart = m_sci->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE,
                                           (unsigned long)m_editState.line);
    long posStart = lineStart + m_editState.spanStart;
    m_sci->SendScintilla(QsciScintillaBase::SCI_GOTOPOS, posStart);

    // Build list from typeName (matches what the editor displays)
    QByteArray list = allTypeNamesForUI().join(' ').toUtf8();
    m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCSETSEPARATOR, (long)' ');
    m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCSETIGNORECASE, (long)1);
    m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCSETDROPRESTOFWORD, (long)1);
    m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCSHOW,
                         (uintptr_t)0, list.constData());

    // Highlight the current type in the list
    QByteArray cur = m_editState.original.toUtf8();
    m_sci->SendScintilla(QsciScintillaBase::SCI_AUTOCSELECT,
                         (uintptr_t)0, cur.constData());

}

// ── Editable-field text-color indicator ──

void RcxEditor::paintEditableSpans(int line) {
    NormalizedSpan norm;
    for (EditTarget t : {EditTarget::Type, EditTarget::Name, EditTarget::Value}) {
        if (resolvedSpanFor(line, t, norm))
            fillIndicatorCols(IND_EDITABLE, line, norm.start, norm.end);
    }
}

void RcxEditor::updateEditableIndicators(int line) {
    if (m_editState.active) return;
    if (line == m_hintLine) return;

    // If new line is selected, its indicators are managed by applySelectionOverlay
    // But we still need to clear the old non-selected hint line
    const LineMeta* newLm = metaForLine(line);
    if (newLm && m_currentSelIds.contains(newLm->nodeId)) {
        if (m_hintLine >= 0) {
            const LineMeta* oldLm = metaForLine(m_hintLine);
            if (!oldLm || !m_currentSelIds.contains(oldLm->nodeId))
                clearIndicatorLine(IND_EDITABLE, m_hintLine);
        }
        m_hintLine = line;
        return;
    }

    // Clear old cursor line (only if not a selected node)
    if (m_hintLine >= 0) {
        const LineMeta* oldLm = metaForLine(m_hintLine);
        if (!oldLm || !m_currentSelIds.contains(oldLm->nodeId))
            clearIndicatorLine(IND_EDITABLE, m_hintLine);
    }

    m_hintLine = line;
    paintEditableSpans(line);
}

// ── Hover cursor ──

void RcxEditor::applyHoverCursor() {
    // Edit mode handles its own cursor (I-beam)
    if (m_editState.active)
        return;

    // Mouse left viewport - set Arrow
    if (!m_hoverInside || !m_sci->viewport()->underMouse()) {
        if (!m_cursorOverridden) {
            QApplication::setOverrideCursor(Qt::ArrowCursor);
            m_cursorOverridden = true;
        } else {
            QApplication::changeOverrideCursor(Qt::ArrowCursor);
        }
        return;
    }

    int line; EditTarget t;
    bool tokenHit = hitTestTarget(m_sci, m_meta, m_lastHoverPos, line, t);

    // Also show pointer cursor for fold column on fold-head lines
    bool interactive = tokenHit;
    if (!interactive) {
        auto h = hitTest(m_lastHoverPos);
        if (h.inFoldCol) interactive = true;
    }

    // Set cursor: pointing hand for interactive, arrow otherwise
    Qt::CursorShape desired = interactive ? Qt::PointingHandCursor : Qt::ArrowCursor;
    if (!m_cursorOverridden) {
        QApplication::setOverrideCursor(desired);
        m_cursorOverridden = true;
    } else {
        QApplication::changeOverrideCursor(desired);
    }
}

// ── Live value validation ──

void RcxEditor::setEditComment(const QString& comment) {
    const LineMeta* lm = metaForLine(m_editState.line);
    if (!lm) return;

    QString lineText = getLineText(m_sci, m_editState.line);
    ColumnSpan cs = commentSpanFor(*lm, lineText.size());
    if (!cs.valid) return;

    // Pad/truncate comment to fixed width
    QString padded = comment.leftJustified(kColComment, ' ').left(kColComment);

    long posA = posFromCol(m_sci, m_editState.line, cs.start);
    long posB = posFromCol(m_sci, m_editState.line, cs.start + kColComment);
    if (posB <= posA) return;

    QByteArray utf8 = padded.toUtf8();
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETTARGETSTART, posA);
    m_sci->SendScintilla(QsciScintillaBase::SCI_SETTARGETEND, posB);
    m_sci->SendScintilla(QsciScintillaBase::SCI_REPLACETARGET,
                         (uintptr_t)utf8.size(), utf8.constData());
}

void RcxEditor::validateEditLive() {
    QString lineText = getLineText(m_sci, m_editState.line);
    int delta = lineText.size() - m_editState.linelenAfterReplace;
    int editedLen = m_editState.original.size() + delta;
    QString text = (editedLen > 0)
        ? lineText.mid(m_editState.spanStart, editedLen).trimmed() : QString();
    QString errorMsg = fmt::validateValue(m_editState.editKind, text);

    // Show/hide error marker (red background) and update comment
    if (errorMsg.isEmpty()) {
        m_sci->markerDelete(m_editState.line, M_ERR);
        setEditComment(QStringLiteral("// Enter=Save Esc=Cancel"));
    } else {
        m_sci->markerAdd(m_editState.line, M_ERR);
        setEditComment(QStringLiteral("// ") + errorMsg);
    }
}

void RcxEditor::setEditorFont(const QString& fontName) {
    g_fontName = fontName;
    QFont f = editorFont();

    m_sci->setFont(f);
    m_lexer->setFont(f);
    for (int i = 0; i <= 127; i++)
        m_lexer->setFont(f, i);
    m_sci->setMarginsFont(f);

    // Re-apply margin styles with new font
    allocateMarginStyles();
}

void RcxEditor::setGlobalFontName(const QString& fontName) {
    g_fontName = fontName;
}

} // namespace rcx
