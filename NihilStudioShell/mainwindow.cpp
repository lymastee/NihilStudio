#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "windows.h"
#include <qpainter.h>
#include <qwindow.h>
#include <qfiledialog.h>

#define NIHIL_CLIENT_PADDING    1

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_idleTimer(this)
{
    ui->setupUi(this);

    QRect rc;
    getClientRect(rc);
    QWidget* f = this->findChild<QWidget*>("clientArea");
    f->setGeometry(rc);
    f->setUpdatesEnabled(false);
    WId wid = f->winId();
    m_core.setup((HWND)wid);
    QObject::connect(&m_idleTimer, SIGNAL(timeout()), this, SLOT(onIdle()));
    m_idleTimer.start(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onIdle()
{
    m_core.render();
}

void MainWindow::on_actionOpen_triggered(bool)
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open", "./", "ALL FILE(*.txt)");
    if (fileName.isEmpty())
        return;
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly|QFile::Text))
        return;
    QString src = file.readAll();
    if (src.isEmpty())
        return;
    wchar_t* pWchar = new wchar_t[src.length()];
    src.toWCharArray(pWchar);
    m_core.loadFromTextStream(gs::string(pWchar));
    delete [] pWchar;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    __super::resizeEvent(event);
    QRect rc;
    getClientRect(rc);
    QWidget* f = this->findChild<QWidget*>("clientArea");
    f->setGeometry(rc);
    m_core.resizeWindow();
}

void MainWindow::getClientRect(QRect &rc) const
{
    auto rcMenu = this->menuWidget()->rect();
    auto rcMain = this->rect();
    rc.setLeft(NIHIL_CLIENT_PADDING);
    rc.setTop(NIHIL_CLIENT_PADDING);
    rc.setWidth(rcMain.width() - NIHIL_CLIENT_PADDING * 2);
    rc.setHeight(rcMain.height() - rcMenu.height() - NIHIL_CLIENT_PADDING * 2);
}

void MainWindow::on_actionShow_Entities_triggered(bool)
{
    m_core.showSolidMode();
}

void MainWindow::on_actionShow_Wireframe_triggered(bool)
{
    m_core.showWireframeMode();
}

void MainWindow::on_actionNavigate_Scene_triggered()
{
    m_core.navigateScene();
}

void MainWindow::on_actionClose_triggered()
{
    close();
}
