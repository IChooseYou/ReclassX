#pragma once
#include "themes/theme.h"
#include <QWidget>
#include <QMenuBar>
#include <QToolButton>
#include <QLabel>
#include <QHBoxLayout>

namespace rcx {

class TitleBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit TitleBarWidget(QWidget* parent = nullptr);

    QMenuBar* menuBar() const { return m_menuBar; }
    void setTitle(const QString& title);
    void applyTheme(const Theme& theme);

    void updateMaximizeIcon();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QMenuBar*    m_menuBar    = nullptr;
    QLabel*      m_titleLabel = nullptr;
    QToolButton* m_btnMin     = nullptr;
    QToolButton* m_btnMax     = nullptr;
    QToolButton* m_btnClose   = nullptr;

    Theme m_theme;

    QToolButton* makeChromeButton(const QString& iconPath);
    void toggleMaximize();
};

} // namespace rcx
