#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <qtimer.h>
#include "core.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void onIdle();
    void on_actionOpen_triggered(bool checked);
    void on_actionShow_Entities_triggered(bool checked);
    void on_actionShow_Wireframe_triggered(bool checked);
    void on_actionNavigate_Scene_triggered();
    void on_actionClose_triggered();
    void on_actionSelect_Objects_triggered();
    void on_actionSelect_Points_triggered();
    void on_actionTranslate_triggered();
    void on_actionRotate_triggered();
    void on_actionScale_triggered();

private:
    Ui::MainWindow *ui;

protected:
    virtual void resizeEvent(QResizeEvent *event) override;

private:
    NihilCore           m_core;
    QWidget*            m_container;
    QTimer              m_idleTimer;

private:
    void getClientRect(QRect& rc) const;
};

#endif // MAINWINDOW_H
