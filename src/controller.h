#pragma once
#include "core.h"
#include "editor.h"
#include "providers/snapshot_provider.h"
#include <QObject>
#include <QUndoStack>
#include <QUndoCommand>
#include <QTimer>
#include <QFutureWatcher>
#include <memory>

class QSplitter;

namespace rcx {

class RcxController;

// ── Document ──

class RcxDocument : public QObject {
    Q_OBJECT
public:
    explicit RcxDocument(QObject* parent = nullptr);

    NodeTree                   tree;
    std::shared_ptr<Provider>  provider;
    QUndoStack                 undoStack;
    QString                    filePath;
    QString                    dataPath;
    bool                       modified = false;

    ComposeResult compose() const;
    bool save(const QString& path);
    bool load(const QString& path);
    void loadData(const QString& binaryPath);
    void loadData(const QByteArray& data);

signals:
    void documentChanged();
};

// ── Undo command ──

class RcxCommand : public QUndoCommand {
public:
    RcxCommand(RcxController* ctrl, Command cmd);
    void undo() override;
    void redo() override;
private:
    RcxController* m_ctrl;
    Command m_cmd;
};

// ── Saved source entry ──

struct SavedSourceEntry {
    QString kind;          // "File" or "Process"
    QString displayName;   // filename or process name
    QString filePath;      // for File sources
    uint32_t pid = 0;      // for Process sources
    QString processName;   // for Process sources
    uint64_t baseAddress = 0;
};

// ── Controller ──

class RcxController : public QObject {
    Q_OBJECT
public:
    explicit RcxController(RcxDocument* doc, QWidget* parent = nullptr);
    ~RcxController() override;

    RcxEditor* primaryEditor() const;
    RcxEditor* addSplitEditor(QSplitter* splitter);
    void removeSplitEditor(RcxEditor* editor);
    QList<RcxEditor*> editors() const { return m_editors; }

    void changeNodeKind(int nodeIdx, NodeKind newKind);
    void renameNode(int nodeIdx, const QString& newName);
    void insertNode(uint64_t parentId, int offset, NodeKind kind, const QString& name);
    void removeNode(int nodeIdx);
    void toggleCollapse(int nodeIdx);
    void setNodeValue(int nodeIdx, int subLine, const QString& text, bool isAscii = false);
    void duplicateNode(int nodeIdx);
    void showContextMenu(RcxEditor* editor, int line, int nodeIdx, int subLine, const QPoint& globalPos);
    void batchRemoveNodes(const QVector<int>& nodeIndices);
    void batchChangeKind(const QVector<int>& nodeIndices, NodeKind newKind);

    void applyCommand(const Command& cmd, bool isUndo);
    void refresh();

    // Selection
    void handleNodeClick(RcxEditor* source, int line, uint64_t nodeId,
                         Qt::KeyboardModifiers mods);
    void clearSelection();
    void applySelectionOverlays();
    QSet<uint64_t> selectedIds() const { return m_selIds; }

    RcxDocument* document() const { return m_doc; }
    void setEditorFont(const QString& fontName);

signals:
    void nodeSelected(int nodeIdx);
    void selectionChanged(int count);

private:
    RcxDocument*       m_doc;
    QList<RcxEditor*>  m_editors;
    ComposeResult      m_lastResult;
    QSet<uint64_t>     m_selIds;
    int                m_anchorLine = -1;
    bool               m_suppressRefresh = false;

    // ── Saved sources for quick-switch ──
    QVector<SavedSourceEntry> m_savedSources;
    int m_activeSourceIdx = -1;

    // ── Auto-refresh state ──
    QTimer*         m_refreshTimer = nullptr;
    QFutureWatcher<QByteArray>* m_refreshWatcher = nullptr;
    std::unique_ptr<SnapshotProvider> m_snapshotProv;
    QByteArray      m_prevSnapshot;
    QSet<int64_t>   m_changedOffsets;
    uint64_t        m_refreshGen = 0;
    uint64_t        m_readGen = 0;
    bool            m_readInFlight = false;

    void connectEditor(RcxEditor* editor);
    void handleMarginClick(RcxEditor* editor, int margin, int line, Qt::KeyboardModifiers mods);
    void updateCommandRow();
    void attachToProcess(uint32_t pid, const QString& processName);
    void switchToSavedSource(int idx);
    void pushSavedSourcesToEditors();

    // ── Auto-refresh methods ──
    void setupAutoRefresh();
    void onRefreshTick();
    void onReadComplete();
    int  computeDataExtent() const;
    void resetSnapshot();
};

} // namespace rcx
