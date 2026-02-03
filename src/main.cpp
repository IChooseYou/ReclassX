#include "controller.h"
#include <QApplication>
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QSplitter>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QAction>
#include <QActionGroup>
#include <QMap>
#include <QTimer>
#include <QDir>
#include <QMetaObject>
#include <QFontDatabase>
#include <QSettings>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>

static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
    fprintf(stderr, "\n=== UNHANDLED EXCEPTION ===\n");
    fprintf(stderr, "Code : 0x%08lX\n", ep->ExceptionRecord->ExceptionCode);
    fprintf(stderr, "Addr : %p\n", ep->ExceptionRecord->ExceptionAddress);

    HANDLE process = GetCurrentProcess();
    HANDLE thread  = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(process, NULL, TRUE);

    CONTEXT* ctx = ep->ContextRecord;
    STACKFRAME64 frame = {};
    DWORD machineType;
#ifdef _M_X64
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = ctx->Rip;
    frame.AddrFrame.Offset = ctx->Rbp;
    frame.AddrStack.Offset = ctx->Rsp;
#else
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = ctx->Eip;
    frame.AddrFrame.Offset = ctx->Ebp;
    frame.AddrStack.Offset = ctx->Esp;
#endif
    frame.AddrPC.Mode    = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    fprintf(stderr, "\nStack trace:\n");
    for (int i = 0; i < 64; i++) {
        if (!StackWalk64(machineType, process, thread, &frame, ctx,
                         NULL, SymFunctionTableAccess64,
                         SymGetModuleBase64, NULL))
            break;
        if (frame.AddrPC.Offset == 0) break;

        char buf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen   = 255;

        DWORD64 disp64 = 0;
        DWORD   disp32 = 0;
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);

        bool hasSym  = SymFromAddr(process, frame.AddrPC.Offset, &disp64, sym);
        bool hasLine = SymGetLineFromAddr64(process, frame.AddrPC.Offset,
                                            &disp32, &line);
        if (hasSym && hasLine) {
            fprintf(stderr, "  [%2d] %s+0x%llx  (%s:%lu)\n",
                    i, sym->Name, (unsigned long long)disp64,
                    line.FileName, line.LineNumber);
        } else if (hasSym) {
            fprintf(stderr, "  [%2d] %s+0x%llx\n",
                    i, sym->Name, (unsigned long long)disp64);
        } else {
            fprintf(stderr, "  [%2d] 0x%llx\n",
                    i, (unsigned long long)frame.AddrPC.Offset);
        }
    }

    SymCleanup(process);
    fprintf(stderr, "=== END CRASH ===\n");
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

namespace rcx {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void loadBinary();

    void addNode();
    void removeNode();
    void changeNodeType();
    void renameNodeAction();
    void splitView();
    void unsplitView();

    void undo();
    void redo();
    void about();
    void setEditorFont(const QString& fontName);

private:
    QMdiArea* m_mdiArea;
    QLabel*   m_statusLabel;

    struct TabState {
        RcxDocument*   doc;
        RcxController* ctrl;
        QSplitter*     splitter;
    };
    QMap<QMdiSubWindow*, TabState> m_tabs;

    void createMenus();
    void createStatusBar();

    RcxController* activeController() const;
    TabState* activeTab();
    QMdiSubWindow* createTab(RcxDocument* doc);
    void updateWindowTitle();
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("ReclassX");
    resize(1200, 800);

    m_mdiArea = new QMdiArea(this);
    m_mdiArea->setViewMode(QMdiArea::TabbedView);
    m_mdiArea->setTabsClosable(true);
    m_mdiArea->setTabsMovable(true);
    setCentralWidget(m_mdiArea);

    createMenus();
    createStatusBar();

    connect(m_mdiArea, &QMdiArea::subWindowActivated,
            this, [this](QMdiSubWindow*) { updateWindowTitle(); });
}

void MainWindow::createMenus() {
    // File
    auto* file = menuBar()->addMenu("&File");
    file->addAction("&New",            QKeySequence::New,    this, &MainWindow::newFile);
    file->addAction("&Open...",        QKeySequence::Open,   this, &MainWindow::openFile);
    file->addSeparator();
    file->addAction("&Save",           QKeySequence::Save,   this, &MainWindow::saveFile);
    file->addAction("Save &As...",     QKeySequence::SaveAs, this, &MainWindow::saveFileAs);
    file->addSeparator();
    file->addAction("Load &Binary...", this, &MainWindow::loadBinary);
    file->addSeparator();
    file->addAction("E&xit",           QKeySequence::Quit,   this, &QMainWindow::close);

    // Edit
    auto* edit = menuBar()->addMenu("&Edit");
    edit->addAction("&Undo", QKeySequence::Undo, this, &MainWindow::undo);
    edit->addAction("&Redo", QKeySequence::Redo, this, &MainWindow::redo);

    // View
    auto* view = menuBar()->addMenu("&View");
    view->addAction("Split &Horizontal", this, &MainWindow::splitView);
    view->addAction("&Unsplit",          this, &MainWindow::unsplitView);
    view->addSeparator();
    auto* fontMenu = view->addMenu("&Font");
    auto* fontGroup = new QActionGroup(this);
    fontGroup->setExclusive(true);
    auto* actConsolas = fontMenu->addAction("Consolas");
    actConsolas->setCheckable(true);
    actConsolas->setActionGroup(fontGroup);
    auto* actIosevka = fontMenu->addAction("Iosevka");
    actIosevka->setCheckable(true);
    actIosevka->setActionGroup(fontGroup);
    // Load saved preference
    QSettings settings("ReclassX", "ReclassX");
    QString savedFont = settings.value("font", "Consolas").toString();
    if (savedFont == "Iosevka") actIosevka->setChecked(true);
    else actConsolas->setChecked(true);
    connect(actConsolas, &QAction::triggered, this, [this]() { setEditorFont("Consolas"); });
    connect(actIosevka, &QAction::triggered, this, [this]() { setEditorFont("Iosevka"); });

    // Node
    auto* node = menuBar()->addMenu("&Node");
    node->addAction("&Add Field",    QKeySequence(Qt::Key_Insert), this, &MainWindow::addNode);
    node->addAction("&Remove Field", QKeySequence::Delete,         this, &MainWindow::removeNode);
    auto* actType = node->addAction("Change &Type", this, &MainWindow::changeNodeType);
    actType->setText("Change &Type\tT");
    auto* actName = node->addAction("Re&name", this, &MainWindow::renameNodeAction);
    actName->setText("Re&name\tF2");

    // Help
    auto* help = menuBar()->addMenu("&Help");
    help->addAction("&About ReclassX", this, &MainWindow::about);
}

void MainWindow::createStatusBar() {
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->setStyleSheet("QStatusBar { background: #252526; color: #858585; }");
}

QMdiSubWindow* MainWindow::createTab(RcxDocument* doc) {
    auto* splitter = new QSplitter(Qt::Horizontal);
    auto* ctrl = new RcxController(doc, splitter);
    ctrl->addSplitEditor(splitter);

    auto* sub = m_mdiArea->addSubWindow(splitter);
    sub->setWindowTitle(doc->filePath.isEmpty()
                        ? "Untitled" : QFileInfo(doc->filePath).fileName());
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->showMaximized();

    m_tabs[sub] = { doc, ctrl, splitter };

    connect(sub, &QObject::destroyed, this, [this, sub]() {
        auto it = m_tabs.find(sub);
        if (it != m_tabs.end()) {
            it->doc->deleteLater();
            m_tabs.erase(it);
        }
    });

    connect(ctrl, &RcxController::nodeSelected,
            this, [this, ctrl](int nodeIdx) {
        if (nodeIdx >= 0 && nodeIdx < ctrl->document()->tree.nodes.size()) {
            auto& node = ctrl->document()->tree.nodes[nodeIdx];
            m_statusLabel->setText(
                QString("%1 %2  offset: +0x%3  size: %4 bytes")
                    .arg(kindToString(node.kind))
                    .arg(node.name)
                    .arg(node.offset, 4, 16, QChar('0'))
                    .arg(node.byteSize()));
        } else {
            m_statusLabel->setText("Ready");
        }
    });

    ctrl->refresh();
    return sub;
}

void MainWindow::newFile() {
    auto* doc = new RcxDocument(this);

    // Autoload self as binary data
    doc->loadData(QCoreApplication::applicationFilePath());
    doc->tree.baseAddress = 0;

    // Read e_lfanew to find PE header offset
    uint32_t lfanew = doc->provider->readU32(0x3C);
    if (lfanew < 0x40 || lfanew >= (uint32_t)doc->provider->size())
        lfanew = 0x40;
    uint32_t pe  = lfanew;          // PE signature
    uint32_t fh  = pe + 4;          // IMAGE_FILE_HEADER
    uint32_t oh  = fh + 20;         // IMAGE_OPTIONAL_HEADER (PE32+)

    Node root;
    root.kind = NodeKind::Struct;
    root.name = "PE_HEADER";
    root.parentId = 0;
    root.offset = 0;
    int ri = doc->tree.addNode(root);
    uint64_t rootId = doc->tree.nodes[ri].id;

    auto add = [&](NodeKind k, const QString& name, int off) {
        Node n;
        n.kind = k;
        n.name = name;
        n.offset = off;
        n.parentId = rootId;
        doc->tree.addNode(n);
    };

    // ── IMAGE_DOS_HEADER (0x00 – 0x3F) ──
    add(NodeKind::UInt16, "e_magic",     0x00);
    add(NodeKind::UInt16, "e_cblp",      0x02);
    add(NodeKind::UInt16, "e_cp",        0x04);
    add(NodeKind::UInt16, "e_crlc",      0x06);
    add(NodeKind::UInt16, "e_cparhdr",   0x08);
    add(NodeKind::UInt16, "e_minalloc",  0x0A);
    add(NodeKind::UInt16, "e_maxalloc",  0x0C);
    add(NodeKind::UInt16, "e_ss",        0x0E);
    add(NodeKind::UInt16, "e_sp",        0x10);
    add(NodeKind::UInt16, "e_csum",      0x12);
    add(NodeKind::UInt16, "e_ip",        0x14);
    add(NodeKind::UInt16, "e_cs",        0x16);
    add(NodeKind::UInt16, "e_lfarlc",    0x18);
    add(NodeKind::UInt16, "e_ovno",      0x1A);
    add(NodeKind::Hex64,  "e_res",       0x1C);
    add(NodeKind::UInt16, "e_oemid",     0x24);
    add(NodeKind::UInt16, "e_oeminfo",   0x26);
    add(NodeKind::Hex64,  "e_res2_0",    0x28);
    add(NodeKind::Hex64,  "e_res2_1",    0x30);
    add(NodeKind::Hex32,  "e_res2_2",    0x38);
    add(NodeKind::UInt32, "e_lfanew",    0x3C);

    // ── DOS Stub (0x40 to PE signature) — fill with Hex nodes ──
    {
        int cursor = 0x40;
        while (cursor + 8 <= (int)pe) {
            add(NodeKind::Hex64,
                QString("stub_%1").arg(cursor, 4, 16, QChar('0')),
                cursor);
            cursor += 8;
        }
        if (cursor + 4 <= (int)pe) {
            add(NodeKind::Hex32,
                QString("stub_%1").arg(cursor, 4, 16, QChar('0')),
                cursor);
            cursor += 4;
        }
        if (cursor + 2 <= (int)pe) {
            add(NodeKind::Hex16,
                QString("stub_%1").arg(cursor, 4, 16, QChar('0')),
                cursor);
            cursor += 2;
        }
        if (cursor + 1 <= (int)pe) {
            add(NodeKind::Hex8,
                QString("stub_%1").arg(cursor, 4, 16, QChar('0')),
                cursor);
            cursor += 1;
        }
    }

    // ── PE Signature ──
    add(NodeKind::UInt32, "Signature",   pe);

    // ── IMAGE_FILE_HEADER (nested struct) ──
    {
        Node fhStruct;
        fhStruct.kind = NodeKind::Struct;
        fhStruct.name = "IMAGE_FILE_HEADER";
        fhStruct.parentId = rootId;
        fhStruct.offset = fh;
        int fi = doc->tree.addNode(fhStruct);
        uint64_t fhId = doc->tree.nodes[fi].id;

        auto addFH = [&](NodeKind k, const QString& name, int off) {
            Node n;
            n.kind = k;
            n.name = name;
            n.offset = off;
            n.parentId = fhId;
            doc->tree.addNode(n);
        };

        addFH(NodeKind::UInt16, "Machine",              0x00);
        addFH(NodeKind::UInt16, "NumberOfSections",     0x02);
        addFH(NodeKind::UInt32, "TimeDateStamp",        0x04);
        addFH(NodeKind::UInt32, "PtrToSymbolTable",     0x08);
        addFH(NodeKind::UInt32, "NumberOfSymbols",      0x0C);
        addFH(NodeKind::UInt16, "SizeOfOptionalHeader", 0x10);
        addFH(NodeKind::UInt16, "Characteristics",      0x12);
    }

    // ── IMAGE_OPTIONAL_HEADER64 (nested struct) ──
    {
        Node ohStruct;
        ohStruct.kind = NodeKind::Struct;
        ohStruct.name = "IMAGE_OPTIONAL_HEADER64";
        ohStruct.parentId = rootId;
        ohStruct.offset = oh;
        int oi = doc->tree.addNode(ohStruct);
        uint64_t ohId = doc->tree.nodes[oi].id;

        auto addOH = [&](NodeKind k, const QString& name, int off) {
            Node n;
            n.kind = k;
            n.name = name;
            n.offset = off;
            n.parentId = ohId;
            doc->tree.addNode(n);
        };

        addOH(NodeKind::UInt16, "Magic",                0x00);
        addOH(NodeKind::UInt8,  "MajorLinkerVersion",   0x02);
        addOH(NodeKind::UInt8,  "MinorLinkerVersion",   0x03);
        addOH(NodeKind::UInt32, "SizeOfCode",           0x04);
        addOH(NodeKind::UInt32, "SizeOfInitData",       0x08);
        addOH(NodeKind::UInt32, "SizeOfUninitData",     0x0C);
        addOH(NodeKind::UInt32, "AddressOfEntryPoint",  0x10);
        addOH(NodeKind::UInt32, "BaseOfCode",           0x14);
        addOH(NodeKind::UInt64, "ImageBase",            0x18);
        addOH(NodeKind::UInt32, "SectionAlignment",     0x20);
        addOH(NodeKind::UInt32, "FileAlignment",        0x24);
        addOH(NodeKind::UInt16, "MajorOSVersion",       0x28);
        addOH(NodeKind::UInt16, "MinorOSVersion",       0x2A);
        addOH(NodeKind::UInt16, "MajorImageVersion",    0x2C);
        addOH(NodeKind::UInt16, "MinorImageVersion",    0x2E);
        addOH(NodeKind::UInt16, "MajorSubsysVersion",   0x30);
        addOH(NodeKind::UInt16, "MinorSubsysVersion",   0x32);
        addOH(NodeKind::UInt32, "Win32VersionValue",    0x34);
        addOH(NodeKind::UInt32, "SizeOfImage",          0x38);
        addOH(NodeKind::UInt32, "SizeOfHeaders",        0x3C);
        addOH(NodeKind::UInt32, "CheckSum",             0x40);
        addOH(NodeKind::UInt16, "Subsystem",            0x44);
        addOH(NodeKind::UInt16, "DllCharacteristics",   0x46);
        addOH(NodeKind::UInt64, "SizeOfStackReserve",   0x48);
        addOH(NodeKind::UInt64, "SizeOfStackCommit",    0x50);
        addOH(NodeKind::UInt64, "SizeOfHeapReserve",    0x58);
        addOH(NodeKind::UInt64, "SizeOfHeapCommit",     0x60);
        addOH(NodeKind::UInt32, "LoaderFlags",          0x68);
        addOH(NodeKind::UInt32, "NumberOfRvaAndSizes",  0x6C);

        // Data directories (16 entries × 8 bytes)
        static const char* dirNames[] = {
            "Export", "Import", "Resource", "Exception",
            "Security", "BaseReloc", "Debug", "Architecture",
            "GlobalPtr", "TLS", "LoadConfig", "BoundImport",
            "IAT", "DelayImport", "CLR", "Reserved"
        };
        for (int i = 0; i < 16; i++) {
            int doff = 0x70 + i * 8;
            addOH(NodeKind::UInt32, QString("%1_RVA").arg(dirNames[i]),  doff);
            addOH(NodeKind::UInt32, QString("%1_Size").arg(dirNames[i]), doff + 4);
        }
    }

    // ── 0x100 bytes of Hex64 padding (32 nodes) ──
    int padStart = oh + 0xF0;  // end of optional header
    for (int i = 0; i < 32; i++) {
        int off = padStart + i * 8;
        add(NodeKind::Hex64,
            QString("pad_%1").arg(off, 4, 16, QChar('0')),
            off);
    }

    createTab(doc);
}

void MainWindow::openFile() {
    QString path = QFileDialog::getOpenFileName(this,
        "Open Definition", {}, "ReclassX (*.rcx);;JSON (*.json);;All (*)");
    if (path.isEmpty()) return;

    auto* doc = new RcxDocument(this);
    if (!doc->load(path)) {
        QMessageBox::warning(this, "Error", "Failed to load: " + path);
        delete doc;
        return;
    }
    createTab(doc);
}

void MainWindow::saveFile() {
    auto* tab = activeTab();
    if (!tab) return;
    if (tab->doc->filePath.isEmpty()) { saveFileAs(); return; }
    tab->doc->save(tab->doc->filePath);
    updateWindowTitle();
}

void MainWindow::saveFileAs() {
    auto* tab = activeTab();
    if (!tab) return;
    QString path = QFileDialog::getSaveFileName(this,
        "Save Definition", {}, "ReclassX (*.rcx);;JSON (*.json)");
    if (path.isEmpty()) return;
    tab->doc->save(path);
    updateWindowTitle();
}

void MainWindow::loadBinary() {
    auto* tab = activeTab();
    if (!tab) return;
    QString path = QFileDialog::getOpenFileName(this,
        "Load Binary Data", {}, "All Files (*)");
    if (path.isEmpty()) return;
    tab->doc->loadData(path);
}

void MainWindow::addNode() {
    auto* ctrl = activeController();
    if (!ctrl) return;

    uint64_t parentId = 0;
    auto* primary = ctrl->primaryEditor();
    if (primary && primary->isEditing()) return;
    if (primary) {
        int ni = primary->currentNodeIndex();
        if (ni >= 0) {
            auto& node = ctrl->document()->tree.nodes[ni];
            if (node.kind == NodeKind::Struct || node.kind == NodeKind::Array)
                parentId = node.id;
            else
                parentId = node.parentId;
        }
    }
    ctrl->insertNode(parentId, -1, NodeKind::Hex64, "newField");
}

void MainWindow::removeNode() {
    auto* ctrl = activeController();
    if (!ctrl) return;
    auto* primary = ctrl->primaryEditor();
    if (!primary || primary->isEditing()) return;
    QSet<int> indices = primary->selectedNodeIndices();
    if (indices.size() > 1) {
        ctrl->batchRemoveNodes(indices.values());
    } else if (indices.size() == 1) {
        ctrl->removeNode(*indices.begin());
    }
}

void MainWindow::changeNodeType() {
    auto* ctrl = activeController();
    if (!ctrl) return;
    auto* primary = ctrl->primaryEditor();
    if (!primary) return;
    primary->beginInlineEdit(EditTarget::Type);
}

void MainWindow::renameNodeAction() {
    auto* ctrl = activeController();
    if (!ctrl) return;
    auto* primary = ctrl->primaryEditor();
    if (!primary) return;
    primary->beginInlineEdit(EditTarget::Name);
}

void MainWindow::splitView() {
    auto* tab = activeTab();
    if (!tab) return;
    tab->ctrl->addSplitEditor(tab->splitter);
}

void MainWindow::unsplitView() {
    auto* tab = activeTab();
    if (!tab) return;
    auto editors = tab->ctrl->editors();
    if (editors.size() > 1)
        tab->ctrl->removeSplitEditor(editors.last());
}

void MainWindow::undo() {
    auto* tab = activeTab();
    if (tab) tab->doc->undoStack.undo();
}

void MainWindow::redo() {
    auto* tab = activeTab();
    if (tab) tab->doc->undoStack.redo();
}

void MainWindow::about() {
    QMessageBox::about(this, "About ReclassX",
        "ReclassX - Structured Binary Editor\n"
        "Built with Qt 6 + QScintilla\n\n"
        "Margin-driven UI with offset display,\n"
        "fold markers, and status flags.");
}

void MainWindow::setEditorFont(const QString& fontName) {
    QSettings settings("ReclassX", "ReclassX");
    settings.setValue("font", fontName);
    // Notify all controllers to refresh fonts
    for (auto& state : m_tabs) {
        state.ctrl->setEditorFont(fontName);
    }
}

RcxController* MainWindow::activeController() const {
    auto* sub = m_mdiArea->activeSubWindow();
    if (sub && m_tabs.contains(sub))
        return m_tabs[sub].ctrl;
    return nullptr;
}

MainWindow::TabState* MainWindow::activeTab() {
    auto* sub = m_mdiArea->activeSubWindow();
    if (sub && m_tabs.contains(sub))
        return &m_tabs[sub];
    return nullptr;
}

void MainWindow::updateWindowTitle() {
    auto* sub = m_mdiArea->activeSubWindow();
    if (sub && m_tabs.contains(sub)) {
        auto& tab = m_tabs[sub];
        QString name = tab.doc->filePath.isEmpty() ? "Untitled"
                       : QFileInfo(tab.doc->filePath).fileName();
        if (tab.doc->modified) name += " *";
        setWindowTitle(name + " - ReclassX");
    } else {
        setWindowTitle("ReclassX");
    }
}

} // namespace rcx

// ── Entry point ──

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(crashHandler);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("ReclassX");
    app.setOrganizationName("ReclassX");
    app.setStyle("Fusion"); // Fusion style respects dark palette well

    // Load embedded Iosevka font
    int fontId = QFontDatabase::addApplicationFont(":/fonts/Iosevka-Regular.ttf");
    if (fontId == -1)
        qWarning("Failed to load embedded Iosevka font");

    // Apply saved font preference before creating any editors
    {
        QSettings settings("ReclassX", "ReclassX");
        QString savedFont = settings.value("font", "Consolas").toString();
        rcx::RcxEditor::setGlobalFontName(savedFont);
    }

    // Global dark palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window,          QColor("#1e1e1e"));
    darkPalette.setColor(QPalette::WindowText,      QColor("#d4d4d4"));
    darkPalette.setColor(QPalette::Base,            QColor("#252526"));
    darkPalette.setColor(QPalette::AlternateBase,   QColor("#2a2d2e"));
    darkPalette.setColor(QPalette::Text,            QColor("#d4d4d4"));
    darkPalette.setColor(QPalette::Button,          QColor("#333333"));
    darkPalette.setColor(QPalette::ButtonText,      QColor("#d4d4d4"));
    darkPalette.setColor(QPalette::Highlight,       QColor("#264f78"));
    darkPalette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    darkPalette.setColor(QPalette::ToolTipBase,     QColor("#252526"));
    darkPalette.setColor(QPalette::ToolTipText,     QColor("#d4d4d4"));
    darkPalette.setColor(QPalette::Mid,             QColor("#3c3c3c"));
    darkPalette.setColor(QPalette::Dark,            QColor("#1e1e1e"));
    darkPalette.setColor(QPalette::Light,           QColor("#505050"));
    app.setPalette(darkPalette);

    rcx::MainWindow window;

    bool screenshotMode = app.arguments().contains("--screenshot");
    if (screenshotMode)
        window.setWindowOpacity(0.0);
    window.show();

    // Always auto-open PE header demo on startup
    QMetaObject::invokeMethod(&window, "newFile");

    if (screenshotMode) {
        QString out = "screenshot.png";
        int idx = app.arguments().indexOf("--screenshot");
        if (idx + 1 < app.arguments().size())
            out = app.arguments().at(idx + 1);

        QTimer::singleShot(1000, [&window, out]() {
            QDir().mkpath(QFileInfo(out).absolutePath());
            window.grab().save(out);
            ::_exit(0);  // immediate exit — no need for clean shutdown in screenshot mode
        });
    }

    return app.exec();
}

#include "main.moc"
