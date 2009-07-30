#include "execwindow.h"
#include "ui_execwindow.h"
#include <time.h>

execWindow::execWindow(QWidget *parent, const QString& info) :
    QDialog(parent),
    m_ui(new Ui::execWindow)
{
    m_ui->setupUi(this);
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_proc,SIGNAL(started()),this,SLOT(onStarted()));
    connect(m_proc,SIGNAL(error(QProcess::ProcessError)),this,SLOT(onError(QProcess::ProcessError)));
    connect(m_proc,SIGNAL(finished(int,QProcess::ExitStatus)),this,SLOT(onFinished(int)));
    connect(m_proc,SIGNAL(readyReadStandardOutput()),this,SLOT(onDataReady()) );

    if(info.isEmpty())
        m_ui->plainTextEdit->appendHtml(tr("<font color=gray>Sony PlayStation3 HD Movie Muxer<br>Copyright (C) 2009 Anton Burdinuk<br>clark15b@gmail.com<br>http://code.google.com/p/tsdemuxer<br>---------------------------------------------------<br><br></font>"));
    else
        m_ui->plainTextEdit->appendHtml(info);
}

execWindow::~execWindow()
{
    delete m_ui;
}

void execWindow::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::LanguageChange:
        m_ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void execWindow::closeEvent(QCloseEvent* e)
{
    if(m_ui->pushButton->isEnabled())
        e->accept();
    else
        e->ignore();
}


void execWindow::run(void)
{
    if(!batch.size())
        return;

    beg_time=time(0);

    NextCmd();

    exec();
}

void execWindow::NextCmd()
{
    if(!batch.size())
    {
        m_ui->pushButton->setEnabled(true);
        m_ui->progressBar->setRange(0,100);
        m_ui->progressBar->setValue(100);
        m_ui->pushButton_2->setEnabled(false);

        if(beg_time>0)
            m_ui->plainTextEdit->appendHtml(tr("<font color=gray>Total time: %1 sec</font>").arg(time(0)-beg_time));
    }else
    {
        execCmd cmd=batch.front();
        batch.pop_front();

        setWindowTitle(cmd.title);

        m_ui->plainTextEdit->appendHtml(tr("<font color=blue>%1</font>").arg(cmd.desc.replace("\n","<br>")));

        m_ui->progressBar->setRange(0,0);
        m_ui->progressBar->setValue(0);

        m_ui->pushButton->setEnabled(false);
        m_ui->pushButton_2->setEnabled(false);

        {
            QString s=cmd.cmd+" ";

            Q_FOREACH(QString p,cmd.params)
                s+=p+" ";
            m_ui->plainTextEdit->appendHtml(QString("<font color=blue>%1</font><br>").arg(s));
        }

        m_proc->start(cmd.cmd,cmd.params);
    }
}

void execWindow::onStarted()
{
    m_ui->progressBar->setRange(0,0);
    m_ui->progressBar->setValue(0);
    m_ui->pushButton->setEnabled(false);
    m_ui->pushButton_2->setEnabled(true);
}
void execWindow::onError(QProcess::ProcessError e)
{
    m_ui->progressBar->setRange(0,100);
    m_ui->progressBar->setValue(100);
    m_ui->pushButton->setEnabled(true);
    m_ui->pushButton_2->setEnabled(false);
    m_ui->plainTextEdit->appendHtml(tr("<font color=red><b>Execute error (%1)</b></font>").arg(e));
}


void execWindow::onFinished(int exitcode)
{
    m_ui->pushButton->setEnabled(false);
    m_ui->progressBar->setRange(0,100);
    m_ui->progressBar->setValue(100);
    m_ui->pushButton_2->setEnabled(false);
    if(m_proc->exitStatus()==QProcess::CrashExit)
    {
        m_ui->plainTextEdit->appendHtml(tr("<font color=red><b>Crash</b></font>"));
        batch.clear();
    }
    else
    {
        if(exitcode)
        {
            m_ui->plainTextEdit->appendHtml(tr("<font color=red><b>FAIL (%1)</b></font>").arg(exitcode));
            batch.clear();
        }
        else
            m_ui->plainTextEdit->appendHtml(tr("<font color=green><b>OK</b></font>"));
    }    
    NextCmd();
}

void execWindow::unescString(const char* buf,int len,QString& s)
{
    std::string ss;
    ss.reserve(len);

    for(int i=0;i<len;i++)
        if(buf[i]>31)
            ss+=buf[i];

    s=QString::fromLocal8Bit(ss.c_str(),ss.length());
}

void execWindow::onDataReady()
{
    //0.2% complete
    //Progress: 1%

    for(;;)
    {
        char buf[1024];
        qint64 n=m_proc->readLine(buf,sizeof(buf));
        if(n==-1 || !n)
            break;

        char* pp=strpbrk(buf,"\r\n");
        if(pp)
        {
            *pp=0;
            n=strlen(buf);
        }

        if(!*buf)
            continue;

        bool show=true;

        if(!strncmp(buf,"Progress: ",10))
        {
            char* p=buf+10;
            char* p2=strchr(p,'%');
            if(p2)
                *p2=0;
            int pos=atoi(p);

            m_ui->progressBar->setRange(0,100);
            m_ui->progressBar->setValue(pos);

            show=false;
        }else
        {
            char* p=buf;
            char* p2=strstr(p,"% complete");
            if(p2)
            {
                *p2=0;
                while(*p && (*p==0x1b || *p==0x4d))
                    p++;

                int pos=atoi(p);

                m_ui->progressBar->setRange(0,100);
                m_ui->progressBar->setValue(pos);

                show=false;
            }
        }

        if(show)
        {
//            QString s=QString::fromLocal8Bit(buf,n);
            QString s;

            unescString(buf,n,s);

            m_ui->plainTextEdit->appendPlainText(s);
            m_ui->plainTextEdit->ensureCursorVisible();
        }
    }
}

void execWindow::on_pushButton_2_clicked()
{
    if(m_proc && m_proc->state()!=QProcess::NotRunning)
        m_proc->kill();
}
