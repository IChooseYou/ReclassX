#include "themeeditor.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QComboBox>

namespace rcx {

// ── Section header label ──

static QLabel* makeSectionLabel(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QStringLiteral(
        "font-weight: bold; font-size: 11px; color: #888;"
        "padding: 6px 0 2px 0; border-bottom: 1px solid #444;"));
    return lbl;
}

// ── Constructor ──

ThemeEditor::ThemeEditor(int themeIndex, QWidget* parent)
    : QDialog(parent), m_themeIndex(themeIndex)
{
    auto& tm = ThemeManager::instance();
    auto all = tm.themes();
    m_theme = (themeIndex >= 0 && themeIndex < all.size()) ? all[themeIndex] : tm.current();

    setWindowTitle(QStringLiteral("Theme Editor"));
    setMinimumSize(420, 480);
    resize(440, 640);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);

    // ── Theme selector combo ──
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(QStringLiteral("Theme:")));
        m_themeCombo = new QComboBox;
        for (const auto& t : all)
            m_themeCombo->addItem(t.name);
        m_themeCombo->setCurrentIndex(themeIndex);
        connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) { loadTheme(idx); });
        row->addWidget(m_themeCombo, 1);
        mainLayout->addLayout(row);
    }

    // ── Name field ──
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(QStringLiteral("Name:")));
        m_nameEdit = new QLineEdit(m_theme.name);
        connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
            m_theme.name = t;
        });
        row->addWidget(m_nameEdit, 1);
        mainLayout->addLayout(row);
    }

    // ── File info ──
    m_fileInfoLabel = new QLabel;
    m_fileInfoLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 10px; padding: 0 0 4px 0;"));
    QString path = tm.themeFilePath(themeIndex);
    m_fileInfoLabel->setText(path.isEmpty()
        ? QStringLiteral("Built-in theme (edits save as user copy)")
        : QStringLiteral("File: %1").arg(path));
    mainLayout->addWidget(m_fileInfoLabel);

    // ── Scrollable area for swatches + contrast ──
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* scrollWidget = new QWidget;
    auto* scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 6, 0);  // right margin for scrollbar
    scrollLayout->setSpacing(2);

    // ── Color swatches ──
    struct FieldDef { const char* label; QColor Theme::*ptr; };

    auto addGroup = [&](const QString& title, std::initializer_list<FieldDef> fields) {
        scrollLayout->addWidget(makeSectionLabel(title));
        for (const auto& f : fields) {
            int idx = m_swatches.size();

            auto* row = new QHBoxLayout;
            row->setSpacing(6);
            row->setContentsMargins(8, 1, 0, 1);

            auto* lbl = new QLabel(QString::fromLatin1(f.label));
            lbl->setFixedWidth(120);
            row->addWidget(lbl);

            auto* swatchBtn = new QPushButton;
            swatchBtn->setFixedSize(32, 18);
            swatchBtn->setCursor(Qt::PointingHandCursor);
            connect(swatchBtn, &QPushButton::clicked, this, [this, idx]() { pickColor(idx); });
            row->addWidget(swatchBtn);

            auto* hexLbl = new QLabel;
            hexLbl->setFixedWidth(60);
            hexLbl->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
            row->addWidget(hexLbl);

            row->addStretch();

            SwatchEntry se;
            se.label = f.label;
            se.field = f.ptr;
            se.swatchBtn = swatchBtn;
            se.hexLabel = hexLbl;
            m_swatches.append(se);

            scrollLayout->addLayout(row);
        }
    };

    addGroup("Chrome", {
        {"Background",     &Theme::background},
        {"Background Alt", &Theme::backgroundAlt},
        {"Surface",        &Theme::surface},
        {"Border",         &Theme::border},
        {"Border Focused", &Theme::borderFocused},
        {"Button",         &Theme::button},
    });
    addGroup("Text", {
        {"Text",        &Theme::text},
        {"Text Dim",    &Theme::textDim},
        {"Text Muted",  &Theme::textMuted},
        {"Text Faint",  &Theme::textFaint},
    });
    addGroup("Interactive", {
        {"Hover",       &Theme::hover},
        {"Selected",    &Theme::selected},
        {"Selection",   &Theme::selection},
    });
    addGroup("Syntax", {
        {"Keyword",      &Theme::syntaxKeyword},
        {"Number",       &Theme::syntaxNumber},
        {"String",       &Theme::syntaxString},
        {"Comment",      &Theme::syntaxComment},
        {"Preprocessor", &Theme::syntaxPreproc},
        {"Type",         &Theme::syntaxType},
    });
    addGroup("Indicators", {
        {"Hover Span",    &Theme::indHoverSpan},
        {"Cmd Pill",      &Theme::indCmdPill},
        {"Data Changed",  &Theme::indDataChanged},
        {"Hint Green",    &Theme::indHintGreen},
    });
    addGroup("Markers", {
        {"Pointer",  &Theme::markerPtr},
        {"Cycle",    &Theme::markerCycle},
        {"Error",    &Theme::markerError},
    });

    scrollLayout->addStretch();
    scroll->setWidget(scrollWidget);
    mainLayout->addWidget(scroll, 1);

    // ── Bottom bar ──
    auto* bottomRow = new QHBoxLayout;
    m_previewBtn = new QPushButton(QStringLiteral("Live Preview"));
    m_previewBtn->setCheckable(true);
    connect(m_previewBtn, &QPushButton::toggled, this, [this](bool) { togglePreview(); });
    bottomRow->addWidget(m_previewBtn);

    bottomRow->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, [this]() {
        if (m_previewing) {
            ThemeManager::instance().revertPreview();
            m_previewing = false;
        }
        reject();
    });
    bottomRow->addWidget(buttons);
    mainLayout->addLayout(bottomRow);

    // Initial update
    for (int i = 0; i < m_swatches.size(); i++)
        updateSwatch(i);
}

// ── Load a different theme into the editor ──

void ThemeEditor::loadTheme(int index) {
    auto& tm = ThemeManager::instance();
    auto all = tm.themes();
    if (index < 0 || index >= all.size()) return;

    m_themeIndex = index;
    m_theme = all[index];
    m_nameEdit->setText(m_theme.name);

    QString path = tm.themeFilePath(index);
    m_fileInfoLabel->setText(path.isEmpty()
        ? QStringLiteral("Built-in theme (edits save as user copy)")
        : QStringLiteral("File: %1").arg(path));

    for (int i = 0; i < m_swatches.size(); i++)
        updateSwatch(i);

    if (m_previewing)
        tm.previewTheme(m_theme);
}

// ── Swatch update ──

void ThemeEditor::updateSwatch(int idx) {
    auto& s = m_swatches[idx];
    QColor c = m_theme.*s.field;

    s.swatchBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid #555; border-radius: 2px; }")
        .arg(c.name()));
    s.hexLabel->setText(c.name());
}

// ── Color picker ──

void ThemeEditor::pickColor(int idx) {
    auto& s = m_swatches[idx];
    QColor c = QColorDialog::getColor(m_theme.*s.field, this, QString::fromLatin1(s.label));
    if (c.isValid()) {
        m_theme.*s.field = c;
        updateSwatch(idx);
            if (m_previewing)
            ThemeManager::instance().previewTheme(m_theme);
    }
}

// ── Live preview toggle ──

void ThemeEditor::togglePreview() {
    m_previewing = m_previewBtn->isChecked();
    if (m_previewing)
        ThemeManager::instance().previewTheme(m_theme);
    else
        ThemeManager::instance().revertPreview();
}

} // namespace rcx
