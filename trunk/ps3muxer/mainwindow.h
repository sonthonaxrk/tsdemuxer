#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include <QTableWidget>
#include <string>
#include <map>

namespace Ui
{
    class MainWindowClass;
}

class codec
{
public:
    std::string name;
    std::string file_ext;
    std::string map;
    int type;   // 1-video, 2-audio
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent, const QString& cmd);
    ~MainWindow();

    QString src_file_name;

private:
    Ui::MainWindowClass *ui;
    void addRow(QTableWidget* w,const QStringList& l);
    std::string source_file_name;
    std::map<std::string,std::string> cfg;
    std::map<std::string,codec> codecs;
    std::string last_dir;

    void initCodec(const std::string& s);
    void parseCmdParams(const QString& s,QStringList& lst);

private slots:
    void on_pushButton_4_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_clicked();
};

#endif // MAINWINDOW_H
