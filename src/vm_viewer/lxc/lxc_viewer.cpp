#include "lxc_viewer.h"

#define BLOCK_SIZE  1024*100
#define TIMEOUT     60*1000
#define PERIOD      333

LXC_Viewer::LXC_Viewer(int startnow, QWidget *parent, virConnect *conn, QString str) :
    QTermWidget(startnow, parent), jobConnect(conn), domain(str)
{
    qRegisterMetaType<QString>("QString&");
    if ( jobConnect!=NULL ) {
        domainPtr = getDomainPtr();
    };
    QString msg;
    if ( domainPtr!=NULL ) {
        stream = virStreamNew( jobConnect, VIR_STREAM_NONBLOCK );
        if ( stream==NULL ) {
            msg = QString("In '<b>%1</b>': Create virtual stream failed...").arg(domain);
            emit errorMsg(msg);
        } else {
            /*
             * Older servers did not support either flag,
             * and also did not forbid simultaneous clients on a console,
             * with potentially confusing results.
             * When passing @flags of 0 in order to support a wider range of server versions,
             * it is up to the client to ensure mutual exclusion.
             */
            int ret;
            if ( ret=virDomainOpenConsole( domainPtr, NULL, stream, VIR_DOMAIN_CONSOLE_SAFE)+1 ) {
                msg = QString("In '<b>%1</b>': Console opened in SAFE-mode...").arg(domain);
                emit errorMsg(msg);
            } else if ( ret=virDomainOpenConsole( domainPtr, NULL, stream, VIR_DOMAIN_CONSOLE_FORCE )+1 ) {
                msg = QString("In '<b>%1</b>': Console opened in FORCE-mode...").arg(domain);
                emit errorMsg(msg);
            } else if ( ret=virDomainOpenConsole( domainPtr, NULL, stream, 0 )+1 ) {
                msg = QString("In '<b>%1</b>': Console opened in ZIRO-mode...").arg(domain);
                emit errorMsg(msg);
            } else {
                msg = QString("In '<b>%1</b>': Open console failed...").arg(domain);
                emit errorMsg(msg);
            };
            if ( ret ) {
                QFont font = QApplication::font();
            #ifdef Q_WS_MAC
                font.setFamily("Monaco");
            #elif defined(Q_WS_QWS)
                font.setFamily("fixed");
            #else
                font.setFamily("Monospace");
            #endif
                font.setPointSize(12);
                this->setTerminalFont(font);
                // usually in a terminals don't used opacity
                //this->setTerminalOpacity(0.5);
                this->setScrollBarPosition(QTermWidget::ScrollBarRight);
                this->setTerminalFont(font);
                //this->setColorScheme(COLOR_SCHEME_BLACK_ON_LIGHT_YELLOW);
                this->setScrollBarPosition(QTermWidget::ScrollBarRight);
                foreach (QString arg, QApplication::arguments()) {
                    if (this->availableColorSchemes().contains(arg))
                        this->setColorScheme(arg);
                    if (this->availableKeyBindings().contains(arg))
                        this->setKeyBindings(arg);
                };
                // don't start (default) shell program,
                // because take the data from VM Stream
                timerId = startTimer(PERIOD);
            };
        };
    } else {
        msg = QString("In '<b>%1</b>': Connect or Domain is NULL...").arg(domain);
        emit errorMsg(msg);
    };
}
LXC_Viewer::~LXC_Viewer()
{
    if ( timerId>0 ) killTimer(timerId);
    if ( domainPtr!=NULL ) {
        if ( readSlaveFd!=NULL ) {
            disconnect(readSlaveFd,
                       SIGNAL(activated(int)),
                       this,
                       SLOT(sendDataToVMachine(int)));
            delete readSlaveFd;
            readSlaveFd = 0;
        };
        closeStream();
    };
    qDebug()<<domain<< "Display destroyed";
}

/* public slots */
void LXC_Viewer::closeTerminal()
{
    closeStream();
    emit jobFinished();
}

/* private slots */
void LXC_Viewer::timerEvent(QTimerEvent *ev)
{
    if ( ev->timerId()==timerId ) {
        ptySlaveFd = this->getSlaveFd();
        counter++;
        //qDebug()<<counter<<ptySlaveFd;
        if ( ptySlaveFd>0 ) {
            killTimer(timerId);
            timerId = 0;
            counter = 0;
            if ( registerStreamEvents()<0 ) {
                QString msg = QString("In '<b>%1</b>': Stream Registation fail.").arg(domain);
                emit errorMsg(msg);
            } else {
                readSlaveFd = new QSocketNotifier(
                            ptySlaveFd,
                            QSocketNotifier::Read);
                connect(readSlaveFd,
                        SIGNAL(activated(int)),
                        this,
                        SLOT(sendDataToVMachine(int)));
                readSlaveFd->setEnabled(true);
                QString msg = QString("In '<b>%1</b>': Stream Registation success. \
PTY opened. Terminal is active.").arg(domain);
                emit errorMsg(msg);
            };
        } else if ( TIMEOUT<counter*PERIOD ) {
            killTimer(timerId);
            timerId = 0;
            counter = 0;
            QString msg = QString("In '<b>%1</b>': Open PTY Error...").arg(domain);
            emit errorMsg(msg);
        }
    }
}
void LXC_Viewer::closeEvent(QCloseEvent *ev)
{
    closeTerminal();
}
virDomain* LXC_Viewer::getDomainPtr() const
{
    return virDomainLookupByName(jobConnect, domain.toUtf8().data());
}
int LXC_Viewer::registerStreamEvents()
{
    int ret = virStreamEventAddCallback(stream,
                                        VIR_STREAM_EVENT_READABLE |
                                        VIR_STREAM_EVENT_HANGUP |
                                        VIR_STREAM_EVENT_ERROR,
                                        streamEventCallBack, this,
    //  don't register freeCallback, because it remove viewer
                                        NULL);
    if (ret<0) sendConnErrors();
    return ret;
}
int LXC_Viewer::unregisterStreamEvents()
{
    int ret = virStreamEventRemoveCallback(stream);
    if (ret<0) sendConnErrors();
    return ret;
}
void LXC_Viewer::freeData(void *opaque)
{
    if ( opaque!=NULL ) {
        void *data = opaque;
        free(data);
    }
}
void LXC_Viewer::streamEventCallBack(virStreamPtr _stream, int events, void *opaque)
{
    LXC_Viewer *obj = static_cast<LXC_Viewer*>(opaque);
    if ( events & VIR_STREAM_EVENT_ERROR ||
         events & VIR_STREAM_EVENT_HANGUP ) {
        // Received stream ERROR/HANGUP, closing console
        obj->closeTerminal();
        return;
    };
    if ( events & VIR_STREAM_EVENT_READABLE ) {
        obj->sendDataToDisplay(_stream);
    };
    if ( events & VIR_STREAM_EVENT_WRITABLE ) {
        //obj->sendDataToVMachine(_stream);
    };
}
void LXC_Viewer::updateStreamEvents(virStreamPtr _stream, int type)
{
    int ret = virStreamEventUpdateCallback(_stream, type);
    if (ret<0) sendConnErrors();
}
void LXC_Viewer::sendDataToDisplay(virStreamPtr _stream)
{
    char buf[BLOCK_SIZE];
    int got = virStreamRecv(_stream, buf, BLOCK_SIZE);
    switch ( got ) {
    case -2:
        // This is basically EAGAIN
        return;
    case 0:
        // Received EOF from stream, closing
        closeTerminal();
        return;
    case -1:
        // Error stream
        return;
    default:
        // send to TermEmulator stdout useing pty->slaveFd
        ::write(ptySlaveFd, &buf, got);
        qDebug()<<QString().fromUtf8(buf)<<"toTerm";
        //QString answer = QString().fromUtf8(buf);
        //display->sendText(answer);
        break;
    };
}
void LXC_Viewer::sendDataToVMachine(int fd)
{
    readSlaveFd->setEnabled(false);
    char buff[BLOCK_SIZE];
    size_t got = read(ptySlaveFd, &buff, BLOCK_SIZE);
    qDebug()<<fd<<"fd"<<buff;
    int saved = virStreamSend(stream, buff, got);
    if ( saved==-2 ) {
        // This is basically EAGAIN
        return;
    } else if ( saved==-1 ) {
        sendConnErrors();
    } else {
        qDebug()<<saved<<"sent";
    };
    updateStreamEvents(stream,
                       VIR_STREAM_EVENT_READABLE |
                       VIR_STREAM_EVENT_ERROR |
                       VIR_STREAM_EVENT_HANGUP);
    readSlaveFd->setEnabled(true);
}
void LXC_Viewer::closeStream()
{
    if ( stream!=NULL ) {
        if ( virStreamEventUpdateCallback(
                 stream,
                 VIR_STREAM_EVENT_READABLE |
                 VIR_STREAM_EVENT_WRITABLE |
                 VIR_STREAM_EVENT_ERROR |
                 VIR_STREAM_EVENT_HANGUP) +1 ) {
            unregisterStreamEvents();
        } else sendConnErrors();
        virStreamFinish(stream);
        virStreamFree(stream);
        stream = NULL;
    };
    qDebug()<<"stream closed";
}

void LXC_Viewer::sendConnErrors()
{
    virtErrors = virConnGetLastError(jobConnect);
    if ( virtErrors!=NULL && virtErrors->code>0 ) {
        QString msg = QString("VirtError(%1) : %2").arg(virtErrors->code)
                .arg(QString().fromUtf8(virtErrors->message));
        emit errorMsg( msg );
        virResetError(virtErrors);
    } else sendGlobalErrors();
}
void LXC_Viewer::sendGlobalErrors()
{
    virtErrors = virGetLastError();
    if ( virtErrors!=NULL && virtErrors->code>0 ) {
        QString msg = QString("VirtError(%1) : %2").arg(virtErrors->code)
                .arg(QString().fromUtf8(virtErrors->message));
        emit errorMsg( msg );
    };
    virResetLastError();
}