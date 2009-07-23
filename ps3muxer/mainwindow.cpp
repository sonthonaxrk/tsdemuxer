#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ebml.h"
#include <QFileDialog>
#include <QMessageBox>
#include "execwindow.h"
#include <stdio.h>
#include <QStyleFactory>

#ifdef _WIN32
#define os_slash    '\\'
#else
#define os_slash    '/'
#endif

#ifdef _WIN32
static const char conf_path[]="ps3muxer_win32.cfg";
#else
static const char conf_path[]="ps3muxer.cfg";
#endif

MainWindow::MainWindow(QWidget *parent,const QString& cmd)
    : QMainWindow(parent), ui(new Ui::MainWindowClass)
{
    ui->setupUi(this);
    ebml::init();
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->setSelectionBehavior ( QAbstractItemView::SelectRows );
    ui->tableWidget_2->setSelectionBehavior ( QAbstractItemView::SelectRows );

    connect(ui->actionOpen,SIGNAL(triggered()),this,SLOT(on_pushButton_clicked()));
    connect(ui->actionClear,SIGNAL(triggered()),this,SLOT(on_pushButton_3_clicked()));
    connect(ui->actionStart_muxing,SIGNAL(triggered()),this,SLOT(on_pushButton_2_clicked()));
    connect(ui->actionQuit,SIGNAL(triggered()),this,SLOT(close()));

    FILE* fp=fopen((QApplication::applicationDirPath()+os_slash+conf_path).toLocal8Bit().data(),"r");
    if(fp)
    {
        char tmp[1024];
        while(fgets(tmp,sizeof(tmp),fp))
        {
            char* p=tmp;

            {
                char* zz=strpbrk(p,"#;\r\n");
                if(zz)
                    *zz=0;
            }

            while(*p && (*p==' ' || *p=='\t'))
                p++;

            if(!*p)
                continue;

            char* p2=strchr(p,'=');
            if(!p2)
                continue;

            char* pp=p2;

            *p2=0;
            p2++;

            while(pp>p && (pp[-1]==' ' || pp[-1]=='\t'))
                pp--;
            *pp=0;

            while(*p2 && (*p2==' ' || *p2=='\t'))
                p2++;

            pp=p2+strlen(p2);

            while(pp>p2 && (pp[-1]==' ' || pp[-1]=='\t'))
                pp--;
            *pp=0;

            if(*p && *p2)
            {
                if(!strncmp(p,"split_",6))
                    ui->comboBox->addItem(p+6,QVariant(p2));
                else if(!strncmp(p,"codec_",6))
                    initCodec(p2);
                else
                    cfg[p]=p2;
            }
        }
        fclose(fp);
    }
    const std::string& style=cfg["style"];
    if(style.length())
        QApplication::setStyle(QStyleFactory::create(style.c_str()));

    src_file_name=cmd;

    if(!src_file_name.isEmpty())
        on_pushButton_clicked();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initCodec(const std::string& s)
{
    std::vector<std::string> ss;
    ss.reserve(3);

    int n=0;
    for(std::string::size_type p1=0,p2;p1!=std::string::npos && n<3;p1=p2,n++)
    {
        std::string c;

        p2=s.find_first_of(':',p1);
        if(p2!=std::string::npos)
        {
            c=s.substr(p1,p2-p1);
            p2++;
        }else
            c=s.substr(p1);

        ss.push_back(c);
    }

    codec cc;

    cc.name=ss[0];
    cc.map=ss[1].length()?ss[1]:cc.name;
    cc.file_ext=ss[2];

    if(!strncmp(cc.name.c_str(),"V_",2))
        cc.type=1;
    else if(!strncmp(cc.name.c_str(),"A_",2))
        cc.type=2;

    if(cc.name.length() && cc.type)
        codecs[cc.name]=cc;
}

void MainWindow::addRow(QTableWidget* w,const QStringList& l)
{
    int column=0;
    int row=w->rowCount();
    w->setRowCount(row+1);
    Q_FOREACH(QString s,l)
    {

        QTableWidgetItem* newItem=new QTableWidgetItem(s);
        w->setItem(row,column++,newItem);
    }
}

void MainWindow::on_pushButton_clicked()
{
    QString path;

    if(src_file_name.size())
    {
        path=src_file_name;
        src_file_name.clear();
    }
    else
        path=QFileDialog::getOpenFileName(this,tr("MKV source file"),last_dir.c_str(),"MKV file (*.mkv)");

#ifdef _WIN32
    path=path.replace('/','\\');
#endif

    if(!path.isEmpty())
    {
        ui->tableWidget->setRowCount(0);
        ui->tableWidget_2->setRowCount(0);

        try
        {
            ebml::stream stream(path.toLocal8Bit().data());

            ebml::doc m;

            ebml::parse(&stream,m);

            if(strcasecmp(m.doc_type.c_str(),"matroska"))
                throw(ebml::exception("not a matroska"));

//            if(m.version!=1 && m.read_version!=1)
//                throw(ebml::exception("unknown matroska version"));

            for(std::map<u_int32_t,ebml::track>::const_iterator i=m.tracks.begin();i!=m.tracks.end();++i)
            {
                const ebml::track& t=i->second;

                QStringList lst;
                lst<<QString("%1").arg(t.id)<<QString("%1").arg(t.timecode==-1?0:t.timecode)<<t.lang.c_str()<<t.codec.c_str();

                const codec& cc=codecs[t.codec];

                if(cc.type==1)
                        addRow(ui->tableWidget,lst);
                else if(cc.type==2)
                        addRow(ui->tableWidget_2,lst);
            }

            if(ui->tableWidget->rowCount()>0)
                ui->tableWidget->setCurrentCell(0,0);

            if(ui->tableWidget_2->rowCount()>0)
                ui->tableWidget_2->setCurrentCell(0,0);

            ui->tableWidget->resizeColumnsToContents();
            ui->tableWidget_2->resizeColumnsToContents();
            ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
            ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);

            {
                std::string s1=path.toLocal8Bit().data();
                std::string s2;

                std::string::size_type n=s1.find_last_of("\\/");
                if(n!=std::string::npos)
                {
                    s2=s1.substr(n+1);
                    s1=s1.substr(0,n);
                }
                else
                {
                    s2=s1;
                    s1="";
                }

                n=s2.find_last_of('.');
                if(n==std::string::npos)
                    s2=s2+".m2ts";
                else
                    s2=s2.substr(0,n)+".m2ts";

                if(s1.length())
                    s1+=os_slash;

                last_dir=s1;

                ui->lineEdit->setText((s1+s2).c_str());
            }

            source_file_name=path.toLocal8Bit().data();
        }
        catch(const std::exception& e)
        {
            QMessageBox::warning(this,tr("Error"),tr("Open file error: %1").arg(e.what()));
        }
    }
}

void MainWindow::on_pushButton_3_clicked()
{
    ui->tableWidget->setRowCount(0);
    ui->tableWidget_2->setRowCount(0);
    ui->lineEdit->clear();
    source_file_name.clear();
}

void MainWindow::on_pushButton_4_clicked()
{
    QString path=QFileDialog::getSaveFileName(this,tr("M2TS target file"),last_dir.c_str(),"M2TS file (*.m2ts)");

#ifdef _WIN32
    path=path.replace('/','\\');
#endif

    if(!path.isEmpty())
    {
        std::string s=path.toLocal8Bit().data();

        std::string::size_type n=s.find_last_of("\\/");
        if(n!=std::string::npos)
            s=s.substr(n+1);

        n=s.find_last_of('.');
        if(n==std::string::npos)
            path=path+".m2ts";

        ui->lineEdit->setText(path);
    }
}

void MainWindow::parseCmdParams(const QString& s,QStringList& lst)
{
    lst<<s.split(' ',QString::SkipEmptyParts);
}

void MainWindow::on_pushButton_2_clicked()
{
    execWindow dlg(this);

    int _video=ui->tableWidget->currentRow();
    int _audio=ui->tableWidget_2->currentRow();

    if(_video==-1 || _audio==-1)
        return;

    std::string video_track_id=ui->tableWidget->item(_video,0)->text().toLocal8Bit().data();
    int         video_delay=atoi(ui->tableWidget->item(_video,1)->text().toLocal8Bit().data());
    std::string video_lang=ui->tableWidget->item(_video,2)->text().toLocal8Bit().data();
    std::string video_codec=ui->tableWidget->item(_video,3)->text().toLocal8Bit().data();

    std::string audio_track_id=ui->tableWidget_2->item(_audio,0)->text().toLocal8Bit().data();
    int         audio_delay=atoi(ui->tableWidget_2->item(_audio,1)->text().toLocal8Bit().data());
    std::string audio_lang=ui->tableWidget_2->item(_audio,2)->text().toLocal8Bit().data();
    std::string audio_codec=ui->tableWidget_2->item(_audio,3)->text().toLocal8Bit().data();

    audio_delay=audio_delay-video_delay;
    video_delay=0;

    std::string split=ui->comboBox->itemData(ui->comboBox->currentIndex()).toString().toLocal8Bit().data();

    std::string tmp_path=cfg["tmp_path"];
    if(!tmp_path.length())
        tmp_path=(QDir::tempPath()+os_slash).toLocal8Bit().data();

    bool remove_tmp_files=true;

    {
        const std::string& s=cfg["remove_tmp_files"];
        if(s=="false" || s=="FALSE" || s=="0")
            remove_tmp_files=false;
        else if(s=="true" || s=="TRUE" || s=="1")
            remove_tmp_files=true;
    }

    std::string audio_file_name;
    std::string audio_file_name2;

    if(strcmp(audio_codec.c_str(),"A_AC3"))
    {
        // transcode to AC3

        QMessageBox mbox(QMessageBox::Question,tr("Transcoding audio"),tr("You have chosen %1 track, it will be converted, process will occupy approximately 15 minutes, you are assured?").arg(audio_codec.c_str()),QMessageBox::Yes|QMessageBox::No,this);
        if(mbox.exec()==QMessageBox::No)
            return;

        audio_file_name=tmp_path+"track_"+audio_track_id+'.';
        audio_file_name2=tmp_path+"track_"+audio_track_id+"_encoded.ac3";

        const codec& cc=codecs[audio_codec];

        audio_file_name+=cc.file_ext.length()?cc.file_ext:"bin";

        QStringList lst;

        lst<<"tracks"<<source_file_name.c_str()<<QString("%1:%2").arg(audio_track_id.c_str()).arg(audio_file_name.c_str());

        dlg.batch.push_back(execCmd(tr("Extracting audio track"),
            tr("Extracting audio track %1 (approximately 3-5 min for 8Gb movie)...").arg(audio_track_id.c_str()),
            cfg["mkvextract"].c_str(),lst));

        lst.clear();

        const std::string& encoder_args=cfg["encoder_args"];

        if(encoder_args.length())
            parseCmdParams(encoder_args.c_str(),lst);

        for(int i=0;i<lst.size();i++)
        {
            QString& s=lst[i];
            if(s=="%1")
                s=audio_file_name.c_str();
            else if(s=="%2")
                s=audio_file_name2.c_str();
        }

        dlg.batch.push_back(execCmd(tr("Encoding audio track"),
            tr("Encoding audio track %1 to AC3 (approximately 10-20 min for 8Gb movie)...").arg(audio_track_id.c_str()),
            cfg["encoder"].c_str(),lst));

    }

    const std::string& meta=tmp_path+cfg["tsmuxer_meta"];

    FILE* fp=fopen(meta.c_str(),"w");
    if(fp)
    {
        std::string opts="MUXOPT --no-pcr-on-video-pid --new-audio-pes --vbr --vbv-len=500";
        if(split.length())
            opts+=" --split-size="+split;
        opts+="\n";

        const std::string& vcc=codecs[video_codec].map;
        const std::string& acc=codecs[audio_codec].map;

        opts+=(vcc.length()?vcc:video_codec)+", \""+source_file_name+"\", insertSEI, contSPS, track="+video_track_id;
        if(video_lang.length())
            opts+=", lang="+video_lang;
        opts+="\n";

        if(audio_file_name2.length())
            opts+="A_AC3, \""+audio_file_name2+"\"";
        else
            opts+=(acc.length()?acc:audio_codec)+", \""+source_file_name+"\", track="+audio_track_id;
        if(audio_lang.length())
            opts+=", lang="+audio_lang;
        if(audio_delay)
        {
            char t[32]; sprintf(t,", timeshift=%ims",audio_delay);
            opts+=t;
        }
        opts+="\n";

        fprintf(fp,"%s",opts.c_str());

        fclose(fp);


        QStringList lst;

        lst<<meta.c_str()<<ui->lineEdit->text();


        dlg.batch.push_back(execCmd(tr("Remux M2TS"),
            tr("Muxing to MPEG2-TS stream (approximately 5 min for 8Gb movie)...\n%1").arg(opts.c_str()),
               cfg["tsmuxer"].c_str(),lst));

        dlg.run();

        if(remove_tmp_files)
        {
            remove(meta.c_str());
            if(audio_file_name.length())
                remove(audio_file_name.c_str());
            if(audio_file_name2.length())
                remove(audio_file_name2.c_str());
        }
    }
}

