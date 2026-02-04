#ifndef PROCESSPICKER_H
#define PROCESSPICKER_H

#include <QDialog>
#include <cstdint>

namespace Ui {
class ProcessPicker;
}

struct ProcessInfo {
    uint32_t pid;
    QString name;
    QString path;
};

class ProcessPicker : public QDialog
{
    Q_OBJECT

public:
    explicit ProcessPicker(QWidget *parent = nullptr);
    ~ProcessPicker();

    uint32_t selectedProcessId() const;
    QString selectedProcessName() const;

private slots:
    void refreshProcessList();
    void onProcessDoubleClicked();

private:
    void enumerateProcesses();
    void populateTable(const QList<ProcessInfo>& processes);

    Ui::ProcessPicker *ui;
    uint32_t m_selectedPid = 0;
    QString m_selectedName;
};

#endif // PROCESSPICKER_H
