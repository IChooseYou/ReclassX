#pragma once
#include <QFrame>
#include <QFont>
#include <QVector>
#include <QString>
#include <cstdint>
#include "core.h"

class QLineEdit;
class QListView;
class QStringListModel;
class QLabel;
class QToolButton;
class QButtonGroup;
class QWidget;

namespace rcx {

struct Theme;

// ── Popup mode ──

enum class TypePopupMode { Root, FieldType, ArrayElement, PointerTarget };

// ── Type entry (explicit discriminant — no sentinel IDs) ──

struct TypeEntry {
    enum Kind { Primitive, Composite, Section };

    Kind        entryKind     = Primitive;
    NodeKind    primitiveKind = NodeKind::Hex8;  // valid when entryKind==Primitive
    uint64_t    structId      = 0;               // valid when entryKind==Composite
    QString     displayName;
    QString     classKeyword;                    // "struct", "class", "enum" (Composite only)
    bool        enabled       = true;            // false = grayed out (visible but not selectable)
};

// ── Parsed type spec (shared between popup filter and inline edit) ──

struct TypeSpec {
    QString baseName;
    bool    isPointer  = false;
    int     ptrDepth   = 0;       // 1 = *, 2 = ** (only meaningful when isPointer)
    int     arrayCount = 0;       // 0 = not array
};

TypeSpec parseTypeSpec(const QString& text);

// ── Popup widget ──

class TypeSelectorPopup : public QFrame {
    Q_OBJECT
public:
    explicit TypeSelectorPopup(QWidget* parent = nullptr);

    void setFont(const QFont& font);
    void setTitle(const QString& title);
    void setMode(TypePopupMode mode);
    void applyTheme(const Theme& theme);
    void setCurrentNodeSize(int bytes);
    void setModifier(int modId, int arrayCount = 0);
    void setTypes(const QVector<TypeEntry>& types, const TypeEntry* current = nullptr);
    void popup(const QPoint& globalPos);

    /// Force native window creation to avoid cold-start delay.
    void warmUp();

signals:
    void typeSelected(const TypeEntry& entry, const QString& fullText);
    void createNewTypeRequested();
    void dismissed();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QLabel*           m_titleLabel   = nullptr;
    QToolButton*      m_escLabel     = nullptr;
    QToolButton*      m_createBtn    = nullptr;
    QLineEdit*        m_filterEdit   = nullptr;
    QLabel*           m_previewLabel = nullptr;
    QListView*        m_listView     = nullptr;
    QStringListModel* m_model        = nullptr;
    QFrame*           m_separator    = nullptr;

    // Modifier toggles
    QWidget*          m_modRow       = nullptr;
    QToolButton*      m_btnPlain     = nullptr;
    QToolButton*      m_btnPtr       = nullptr;
    QToolButton*      m_btnDblPtr    = nullptr;
    QToolButton*      m_btnArray     = nullptr;
    QLineEdit*        m_arrayCountEdit = nullptr;
    QButtonGroup*     m_modGroup     = nullptr;

    QVector<TypeEntry> m_allTypes;
    QVector<TypeEntry> m_filteredTypes;
    TypeEntry          m_currentEntry;
    bool               m_hasCurrent = false;
    TypePopupMode      m_mode = TypePopupMode::FieldType;
    int                m_currentNodeSize = 0;
    QFont              m_font;

    void applyFilter(const QString& text);
    void updateModifierPreview();
    void acceptCurrent();
    void acceptIndex(int row);
    int  nextSelectableRow(int from, int direction) const;
};

} // namespace rcx
