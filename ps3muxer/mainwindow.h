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
    std::string print_name;
    std::string file_ext;
    std::string map;
    int type;   // 1-video, 2-audio
};

class track_info
{
public:
    std::string track_id;
    int         delay;
    std::string lang;
    std::string codec;
    std::string filename;

    track_info(void):delay(0) {}
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
    std::map<std::string,int> native_codecs;
    std::string last_dir;

    void initCodec(const std::string& s,const std::string pn);
    void parseCmdParams(const QString& s,QStringList& lst);
    void startMuxing();

private slots:
    void on_tableWidget_itemSelectionChanged();
    void on_tableWidget_2_itemSelectionChanged();
    void on_pushButton_4_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_clicked();
    void aboutBox();
};

#endif // MAINWINDOW_H
