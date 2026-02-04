#include "processpicker.h"
#include "ui_processpicker.h"
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QMessageBox>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

ProcessPicker::ProcessPicker(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ProcessPicker)
{
    ui->setupUi(this);
    
    // Configure table
    ui->processTable->horizontalHeader()->setStretchLastSection(true);
    ui->processTable->setColumnWidth(0, 80);   // PID column
    ui->processTable->setColumnWidth(1, 200);  // Name column
    
    // Connect signals
    connect(ui->refreshButton, &QPushButton::clicked, this, &ProcessPicker::refreshProcessList);
    connect(ui->processTable, &QTableWidget::itemDoubleClicked, this, &ProcessPicker::onProcessDoubleClicked);
    
    // Initial process enumeration
    refreshProcessList();
}

ProcessPicker::~ProcessPicker()
{
    delete ui;
}

uint32_t ProcessPicker::selectedProcessId() const
{
    return m_selectedPid;
}

QString ProcessPicker::selectedProcessName() const
{
    return m_selectedName;
}

void ProcessPicker::refreshProcessList()
{
    ui->processTable->clearContents();
    ui->processTable->setRowCount(0);
    enumerateProcesses();
}

void ProcessPicker::onProcessDoubleClicked()
{
    auto* item = ui->processTable->currentItem();
    if (!item) return;
    
    int row = item->row();
    m_selectedPid = ui->processTable->item(row, 0)->data(Qt::UserRole).toUInt();
    m_selectedName = ui->processTable->item(row, 1)->text();
    
    accept();
}

void ProcessPicker::enumerateProcesses()
{
    QList<ProcessInfo> processes;
    
#ifdef _WIN32
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        QMessageBox::warning(this, "Error", "Failed to enumerate processes.");
        return;
    }
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    if (Process32FirstW(snapshot, &pe32)) {
        do {
            ProcessInfo info;
            info.pid = pe32.th32ProcessID;
            info.name = QString::fromWCharArray(pe32.szExeFile);
            
            // Try to get full path
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                WCHAR path[MAX_PATH];
                DWORD pathLen = MAX_PATH;
                if (QueryFullProcessImageNameW(hProcess, 0, path, &pathLen)) {
                    info.path = QString::fromWCharArray(path);
                } else {
                    info.path = "";
                }
                CloseHandle(hProcess);
            } else {
                info.path = "";
            }
            
            processes.append(info);
            
        } while (Process32NextW(snapshot, &pe32));
    }
    
    CloseHandle(snapshot);
#else
    // Platform not supported
    QMessageBox::warning(this, "Error", "Process enumeration not supported on this platform.");
#endif
    
    populateTable(processes);
}

void ProcessPicker::populateTable(const QList<ProcessInfo>& processes)
{
    ui->processTable->setRowCount(processes.size());
    
    for (int i = 0; i < processes.size(); ++i) {
        const auto& proc = processes[i];
        
        // PID column
        auto* pidItem = new QTableWidgetItem();
        pidItem->setData(Qt::EditRole, (int)proc.pid);
        ui->processTable->setItem(i, 0, pidItem);
        
        // Name column
        ui->processTable->setItem(i, 1, new QTableWidgetItem(proc.name));
        
        // Path column
        ui->processTable->setItem(i, 2, new QTableWidgetItem(proc.path));
    }
    
    ui->processTable->resizeColumnsToContents();
    ui->processTable->horizontalHeader()->setStretchLastSection(true);
}
