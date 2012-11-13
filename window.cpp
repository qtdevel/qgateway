/*
 *   This file is part of QGateway.
 *
 *   QGateway is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   QGateway is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with QGateway.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Copyright (C) 2012 Roman Ustyugov.
 */

#include <QtGui>
#include <QtCore/QProcess>

#include "window.h"
#include "ping.h"

#ifdef HAVE_MS_PING
#include "ms_ping.h"
#endif

const QString GOOGLE_DNS = "8.8.8.8";

//
//  Validator for checking string to be valid IP address
//
class IP4Validator : public QValidator
{
public:
    IP4Validator(QObject *parent=0) : QValidator(parent){}
    void fixup(QString &) const {}
    State validate(QString &input, int &) const {
        if(input.isEmpty()) return Acceptable;
        QStringList slist = input.split(".");
        int s = slist.size();
        if(s>4) return Invalid;
        bool emptyGroup = false;
        for(int i=0;i<s;i++){
            bool ok;
            if(slist[i].isEmpty()){
                emptyGroup = true;
                continue;
            }
            int val = slist[i].toInt(&ok);
            if(!ok || val<0 || val>255) return Invalid;
        }
        if(s<4 || emptyGroup) return Intermediate;
        return Acceptable;
    }
};

//------------------------------------------------------------------------------------------------
//################################################################################################
//------------------------------------------------------------------------------------------------
Window::Window()
{
    errorMessage = new QErrorMessage(this);

    createTabs();

    createActions();
    createTrayIcon();

    connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(tabs);

    QHBoxLayout *bl = new QHBoxLayout;

    quitButton = new QPushButton(QIcon(":/images/door-open-out.png"), tr("Quit"));
    quitButton->setAutoDefault(false);
    quitButton->setFocusPolicy(Qt::NoFocus);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(quitApp()));
    bl->addWidget(quitButton, 0, Qt::AlignLeft);

    aboutButton = new QPushButton(QIcon(":/images/question-balloon.png"), tr("About"));
    aboutButton->setAutoDefault(false);
    aboutButton->setFocusPolicy(Qt::NoFocus);
    connect(aboutButton, SIGNAL(clicked()), this, SLOT(about()));
    bl->addWidget(aboutButton, 0, Qt::AlignCenter);

    closeButton = new QPushButton(QIcon(":/images/tick.png"), tr("Close"));
    closeButton->setAutoDefault(false);
    closeButton->setFocusPolicy(Qt::NoFocus);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeWanted()));
    bl->addWidget(closeButton, 0, Qt::AlignRight);

    mainLayout->addLayout(bl);

    setLayout(mainLayout);

    pingTimer = new QTimer(this);
    connect(pingTimer, SIGNAL(timeout()), this, SLOT(pingProc()));
    switchTimer = new QTimer(this);
    connect(switchTimer, SIGNAL(timeout()), this, SLOT(switchProc()));
    prioTimer = new QTimer(this);
    connect(prioTimer, SIGNAL(timeout()), this, SLOT(prioProc()));

    trayIcon->show();
    setWindowTitle("QGateway");

    readSettings();

    settChanged = false;
    startupChanged = true;
    checkStartup();

    setWindowIcon(QIcon(":/images/computer-network.png"));
}
//------------------------------------------------------------------------------------------------
Window::~Window()
{
    if (writeGwOnExit->isChecked())
        writeGateway();
}
//------------------------------------------------------------------------------------------------
void Window::writeGateway()
{
#ifdef Q_OS_WIN
    if (!currentGw.isEmpty())
    {
        QProcess *route = new QProcess(this);
        QString program = "route";

        QStringList arguments;
        arguments << "delete" << "0.0.0.0";
        route->start(program, arguments);
        route->waitForFinished();

        arguments.clear();
        arguments << "-p" << "add" << "0.0.0.0" << "mask" << "0.0.0.0" << currentGw;
        route->start(program, arguments);
        route->waitForFinished();

        route->deleteLater();
    }
#endif
}
//------------------------------------------------------------------------------------------------
void Window::createTabs()
{
    tabs = new QTabWidget;
    addOptionsTab();
    addGatewaysTab();
}
//------------------------------------------------------------------------------------------------
void Window::closeWanted()
{
    hide();
    applySettings();
    writeSettings();
}
//------------------------------------------------------------------------------------------------
void Window::showdlg()
{
    show();
    raise();
    activateWindow();
}
//------------------------------------------------------------------------------------------------
void Window::addOptionsTab()
{
    QWidget * w = new QWidget;
    switchType = new QComboBox;
    switchType->addItem(tr("auto switching"));
    switchType->addItem(tr("upper gateway in list is more preferrable"));
    switchType->addItem(tr("use any available gateway"));
    switchType->addItem(tr("do not use gateway"));
    connect(switchType, SIGNAL(currentIndexChanged(int)),
            this, SLOT(switchTypeChanged(int)));
    pingHostEdit = new QLineEdit(GOOGLE_DNS);
    pingHostEdit->setValidator(new IP4Validator(this));
    connect(pingHostEdit, SIGNAL(textEdited(const QString &)),
            this, SLOT(textEdited(const QString &)));
    QFormLayout *fl = new QFormLayout;
    fl->addRow(tr("Switching method:"), switchType);
    fl->addRow(tr("IP address to ping:"), pingHostEdit);
    pingTimeout = new QSpinBox;
    pingTimeout->setMinimum(1);
    pingTimeout->setMaximum(1000);
    pingTimeout->setSuffix(tr(" ms."));
    pingTimeout->setMaximumWidth(70);
    connect(pingTimeout, SIGNAL(valueChanged(int)),
            this, SLOT(spinChanged(int)));
    fl->addRow(tr("Maximum of ping timeout:"), pingTimeout);
    connTimeout = new QSpinBox;
    connTimeout->setMinimum(5);
    connTimeout->setSuffix(tr(" sec."));
    connTimeout->setMaximumWidth(70);
    connect(connTimeout, SIGNAL(valueChanged(int)),
            this, SLOT(spinChanged(int)));
    fl->addRow(tr("Switch if connection timeout is:"), connTimeout);
    prioTimeout = new QSpinBox;
    prioTimeout->setMinimum(1);
    prioTimeout->setSuffix(tr(" min."));
    prioTimeout->setMaximumWidth(70);
    connect(prioTimeout, SIGNAL(valueChanged(int)),
            this, SLOT(spinChanged(int)));
    QHBoxLayout * hl = new QHBoxLayout;
    forceSwitchButton = new QPushButton(QIcon(":/images/arrow-circle-double.png"),"");
    forceSwitchButton->setToolTip(tr("Force switching right now"));
    forceSwitchButton->setMaximumWidth(30);
    forceSwitchButton->setAutoDefault(false);
    forceSwitchButton->setFocusPolicy(Qt::NoFocus);
    connect(forceSwitchButton, SIGNAL(clicked()),
            this, SLOT(forceSwitch()));
    hl->addWidget(prioTimeout);
    hl->addWidget(forceSwitchButton);
    fl->addRow(tr("Priority switching after:"), hl);
    runOnStartup = new QCheckBox;
#ifdef Q_OS_WIN
    fl->addRow(tr("Run program on system startup:"), runOnStartup);
#endif
    connect(runOnStartup, SIGNAL(stateChanged(int)),
            this, SLOT(startupChecked(int)));
    showBalloons = new QCheckBox;
    connect(showBalloons, SIGNAL(stateChanged(int)),
            this, SLOT(checkboxChanged(int)));
    fl->addRow(tr("Show balloons:"), showBalloons);
    askOnQuit = new QCheckBox;
    connect(askOnQuit, SIGNAL(stateChanged(int)),
            this, SLOT(checkboxChanged(int)));
    fl->addRow(tr("Ask on quit:"), askOnQuit);
    writeGwOnExit = new QCheckBox;
    connect(writeGwOnExit, SIGNAL(stateChanged(int)),
            this, SLOT(checkboxChanged(int)));
#ifdef Q_OS_WIN
    fl->addRow(tr("Write gateway to registry on exit:"), writeGwOnExit);
#endif
    QVBoxLayout *vl = new QVBoxLayout;
    vl->addLayout(fl);
    vl->addSpacing(15);
    gwInfo = new QLabel;
    vl->addWidget(gwInfo);
    w->setLayout(vl);
    tabs->addTab(w, QIcon(":/images/wrench.png"), tr("Options"));
}
//------------------------------------------------------------------------------------------------
void Window::readSettings()
{
    setInetStatus(imSwitching);

    QSettings settings(iniFileName(), QSettings::IniFormat);
    int val;
    QString key;

    key = "Options/";
    val = settings.value(key + "ConnectionTimeout", QVariant(8)).toInt();
    if (val < connTimeout->minimum())
        val = connTimeout->minimum();
    if (val > connTimeout->maximum())
        val = connTimeout->maximum();
    connTimeout->setValue(val);
    val = settings.value(key + "PriorityTimeout", QVariant(10)).toInt();
    if (val < prioTimeout->minimum())
        val = prioTimeout->minimum();
    if (val > prioTimeout->maximum())
        val = prioTimeout->maximum();
    prioTimeout->setValue(val);
    val = settings.value(key + "PingTimeout", QVariant(800)).toInt();
    if (val < pingTimeout->minimum())
        val = pingTimeout->minimum();
    if (val > pingTimeout->maximum())
        val = pingTimeout->maximum();
    pingTimeout->setValue(val);

    bool bval;
    bval = settings.value(key + "RunOnStartup", QVariant(false)).toBool();
    runOnStartup->setChecked(bval);

    bval = settings.value(key + "ShowBalloons", QVariant(true)).toBool();
    showBalloons->setChecked(bval);

    bval = settings.value(key + "AskOnQuit", QVariant(true)).toBool();
    askOnQuit->setChecked(bval);

    bval = settings.value(key + "WriteGatewayOnExit", QVariant(true)).toBool();
    writeGwOnExit->setChecked(bval);

    indexGateway = -1;

    canSwitch = (gwList->count() > 1);

    pingHost = settings.value(key + "PingHost", QVariant("8.8.8.8")).toString();
    if (pingHost.isEmpty() || !isValidIP(pingHost))
        pingHost = GOOGLE_DNS;
    pingHostEdit->setText(pingHost);

    key = "Gateways/";
    QStringList defaultGateways;
    defaultGateways << "172.16.10.1" << "172.16.10.5";

    //  avoid redundant calling slot
    disconnect(switchType, SIGNAL(currentIndexChanged(int)),
            this, SLOT(switchTypeChanged(int)));
    //  remove gateways from combobox
    int c = switchType->count();
    for (int i = 4; i < c; i++)
        switchType->removeItem(4);
    switchType->setCurrentIndex(-1);
    //  connect again
    connect(switchType, SIGNAL(currentIndexChanged(int)),
            this, SLOT(switchTypeChanged(int)));

    QStringList gateways = settings.value(key + "GatewayList", QVariant(defaultGateways)).toStringList();

    gwList->addItems(gateways);
    switchType->addItems(gateways);
    gwListChanged = false;

    key = "MainWindow/";
    resize((const QSize &)settings.value(key + "Size", QVariant(QSize(550, 300))));

    key = "Options/";
    val = settings.value(key + "SwitchType", QVariant(0)).toInt();
    if (val < 0)
        val = 0;
    if (val >= switchType->count())
        val = 0;
    switchType->setCurrentIndex(val);
}
//------------------------------------------------------------------------------------------------
QString Window::iniFileName()
{
    QString fn = QCoreApplication::applicationDirPath();
    fn += QDir::separator();
    fn += "qgateway.ini";
    return fn;
}
//------------------------------------------------------------------------------------------------
void Window::startupChecked(int)
{
    startupChanged = true;
    settingsChanged();
}
//------------------------------------------------------------------------------------------------
void Window::checkStartup()
{
    if (startupChanged)
    {
#ifdef Q_OS_WIN
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                            QSettings::NativeFormat);
        QString key = "QGateway";
        QString appPath = QCoreApplication::applicationFilePath().replace('/','\\');
        bool hasValue = (settings.value(key) == appPath);
        if (runOnStartup->isChecked())
        {
            if (!hasValue)
                settings.setValue(key, appPath);
        }
        else
        {
            if (hasValue)
                settings.remove(key);
        }
#endif
        startupChanged = false;
    }
}
//------------------------------------------------------------------------------------------------
void Window::checkButtons()
{
    if (selGw.count())
    {
        int row = gwList->row(selGw.at(0));
        upGw->setEnabled(row > 0);
        downGw->setEnabled(row < gwList->count() - 1);
    }
    else
    {
        upGw->setEnabled(false);
        downGw->setEnabled(false);
    }
    delGw->setEnabled(selGw.count());
}
//------------------------------------------------------------------------------------------------
void Window::applySwitchMode()
{
    int index = switchType->currentIndex();
    switch (index)
    {
        case 0:
            switchMode = mAutoSwitch;
            break;
        case 1:
            switchMode = mUpperGateway;
            break;
        case 2:
            switchMode = mAnyGateway;
            break;
        case 3:
            switchMode = mNoGateway;
            break;
        default:
            switchMode = mOnlyGateway;
            break;
    }

    switch (switchMode)
    {
        case mNoGateway:
            prioTimer->stop();
            switchTimer->stop();
            pingTimer->stop();
            canSwitch = false;
            currentGw.clear();
            runClearGateway();
            forceSwitchButton->setEnabled(false);
            break;
        case mAutoSwitch:
        case mUpperGateway:
        case mAnyGateway:
            indexGateway = -1;
            canSwitch = (gwList->count() > 1);
            switchProc();
            break;
        case mOnlyGateway:
            {
                prioTimer->stop();
                switchTimer->stop();
                canSwitch = false;
                QListWidgetItem * item = gwList->item(index - 4);
                if (item)
                    switchTo(item->text());
            } break;
    }
}
//------------------------------------------------------------------------------------------------
void Window::forceSwitch()
{
    switch (switchMode)
    {
        case mAutoSwitch:
        case mUpperGateway:
        case mAnyGateway:
            indexGateway = -1;
            canSwitch = (gwList->count() > 1);
            if (switchMode == mAutoSwitch)
                prioTimeoutValue = prioTimeout->value();
            switchProc();
            break;
    }
}
//------------------------------------------------------------------------------------------------
void Window::applySettings()
{
    pingHost = pingHostEdit->text();
    if (pingHost.isEmpty() || !isValidIP(pingHost))
    {
        pingHost = GOOGLE_DNS;
        pingHostEdit->setText(GOOGLE_DNS);
    }

    checkStartup();
}
//------------------------------------------------------------------------------------------------
void Window::writeSettings()
{
    if (settChanged)
    {
        QSettings settings(iniFileName(), QSettings::IniFormat);

        settings.beginGroup("Options");
        settings.setValue("SwitchType", switchType->currentIndex());
        settings.setValue("ConnectionTimeout", connTimeout->value());
        settings.setValue("PriorityTimeout", prioTimeout->value());
        settings.setValue("PingTimeout", pingTimeout->value());
        settings.setValue("RunOnStartup", runOnStartup->isChecked());
        settings.setValue("ShowBalloons", showBalloons->isChecked());
        settings.setValue("AskOnQuit", askOnQuit->isChecked());
        settings.setValue("WriteGatewayOnExit", writeGwOnExit->isChecked());
        settings.setValue("PingHost", pingHost);
        settings.endGroup();

        settings.beginGroup("Gateways");
        QStringList gateways;
        QListWidgetItem * item;
        for (int i = 0; i < gwList->count(); i++)
        {
            item = gwList->item(i);
            if (item)
                gateways << item->text();
        }
        settings.setValue("GatewayList", gateways);
        settings.endGroup();

        settings.beginGroup("MainWindow");
        settings.setValue("Size", size());
        settings.endGroup();
        settChanged = false;
    }
}
//------------------------------------------------------------------------------------------------
void Window::switchTypeChanged(int index)
{
    applySwitchMode();
    connTimeout->setEnabled(index < 3);
    pingTimeout->setEnabled(switchMode != mNoGateway);
    prioTimeout->setEnabled(index < 2);
    settingsChanged();
}
//------------------------------------------------------------------------------------------------
void Window::addGatewaysTab()
{
    QWidget * w = new QWidget;

    gwEdit = new QLineEdit;
    gwEdit->setValidator(new IP4Validator(this));
    connect(gwEdit, SIGNAL(textChanged(const QString &)),
            this, SLOT(gwTextChanged(const QString &)));

    addGw = new QPushButton;
    addGw->setIcon(QIcon(":/images/plus.png"));
    connect(addGw, SIGNAL(clicked()), this, SLOT(addHost()));
    addGw->setEnabled(false);

    QHBoxLayout * h1 = new QHBoxLayout;
    h1->addWidget(gwEdit);
    h1->addWidget(addGw);

    gwList = new QListWidget;
    gwList->setSelectionMode(QAbstractItemView::SingleSelection);
    gwList->setSortingEnabled(false);
    connect(gwList, SIGNAL(itemSelectionChanged()),
              this, SLOT(gatewaySelectionChanged()));

    upGw = new QPushButton;
    upGw->setIcon(QIcon(":/images/arrow-up.png"));
    connect(upGw, SIGNAL(clicked()), this, SLOT(upHost()));

    downGw = new QPushButton;
    downGw->setIcon(QIcon(":/images/arrow-down.png"));
    connect(downGw, SIGNAL(clicked()), this, SLOT(downHost()));

    delGw = new QPushButton;
    delGw->setIcon(QIcon(":/images/minus.png"));
    connect(delGw, SIGNAL(clicked()), this, SLOT(delHost()));

    QGridLayout * g = new QGridLayout;
    g->addWidget(gwList, 0, 0, 4, 1);
    g->addWidget(upGw, 0, 1);
    g->addWidget(downGw, 1, 1);
    g->addWidget(delGw, 2, 1);

    QVBoxLayout * vl = new QVBoxLayout;
    vl->addLayout(h1);
    vl->addSpacing(10);
    vl->addLayout(g);
    w->setLayout(vl);
    tabs->addTab(w, QIcon(":/images/network-ip-local.png"), tr("Gateways"));
    checkButtons();
}
//------------------------------------------------------------------------------------------------
void Window::gatewaySelectionChanged()
{
    selGw = gwList->selectedItems();
    checkButtons();
}
//------------------------------------------------------------------------------------------------
void Window::gwTextChanged(const QString & text)
{
    addGw->setEnabled(isValidIP(text));
}
//------------------------------------------------------------------------------------------------
bool Window::isValidIP(const QString & ip)
{
    QString Octet = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegExp rxip("^" + Octet + "\\." + Octet + "\\." + Octet + "\\." + Octet + "$");
    return rxip.exactMatch(ip);
}
//------------------------------------------------------------------------------------------------
void Window::runClearGateway()
{
    //  stop ping before switching gateway
    pingTimer->stop();

    QProcess *route = new QProcess(this);
    QString program = "route";

    QStringList arguments;
#ifdef Q_OS_WIN
    arguments << "delete" << "0.0.0.0";
#endif
    route->start(program, arguments);

    route->waitForFinished();
    route->deleteLater();
    currentGw.clear();
    trayIcon->setToolTip(tr("Gateway: none"));
    gwInfo->setText(tr("Current gateway: ") + "<b>" + tr("none") + "</b>");
    setInetStatus(imNoGateway);
    if (showBalloons->isChecked())
        trayIcon->showMessage(tr("Current gateway:"), tr("none"), QSystemTrayIcon::Information, 5000);
}
//------------------------------------------------------------------------------------------------
void Window::runAddGateway(QString gw)
{
    QProcess *route = new QProcess(this);
    QString program = "route";

    QStringList arguments;
#ifdef Q_OS_WIN
    arguments << "add" << "0.0.0.0" << "mask" << "0.0.0.0" << gw;
#endif
    route->start(program, arguments);

    route->waitForFinished();

    //  start ping every 2 seconds
    pingTimer->start(2000);

    QString tooltip;
    if (route->exitCode() == 0)
    {
        if (showBalloons->isChecked())
            trayIcon->showMessage(tr("Current gateway:"), gw, QSystemTrayIcon::Information, 5000);
        routeError.clear();
        currentGw = gw;
        tooltip = tr("Gateway:") + " " + currentGw;
        gwInfo->setText(tr("Current gateway: ") + "<b>" + currentGw + "</b>");
    }
    else
    {
        if (showBalloons->isChecked())
            trayIcon->showMessage(tr("Cannot switch gateway"), tr("Press here for additional information"), QSystemTrayIcon::Critical, 5000);
#ifdef Q_OS_WIN
        QByteArray ba = route->readAllStandardError();
        QTextCodec *codec = QTextCodec::codecForName("IBM866");
        routeError = codec->toUnicode(ba);
#else
        routeError = route->readAllStandardError();
#endif
        tooltip = tr("Gateway: none");
        gwInfo->setText(tr("Current gateway: ") + "<b>" + tr("none") + "</b>");
    }
    trayIcon->setToolTip(tooltip);

    route->deleteLater();
}
//------------------------------------------------------------------------------------------------
void Window::switchTo(QString gw)
{
    if (currentGw == gw)
        return;
    switchTimer->stop();
    prioTimer->stop();

    runClearGateway();
    runAddGateway(gw);
}
//------------------------------------------------------------------------------------------------
void Window::messageClicked()
{
    if (!routeError.isEmpty())
        errorMessage->showMessage(routeError);
}
//------------------------------------------------------------------------------------------------
void Window::pingProc()
{
    QString info;
    info = tr("Current gateway: ") + "<b>" + currentGw + "</b>";
    info += ", ";

    int res;
#ifdef HAVE_MS_PING
    if (canUseMsIcmp)
        res = ms_ping(pingHost.toLatin1(), pingTimeout->value());
    else
        res = ping(pingHost.toLatin1(), pingTimeout->value());
#else
    res = ping(pingHost.toLatin1(), pingTimeout->value());
#endif
    if (res <= 0)
    {
        if (!switchTimer->isActive() && canSwitch)
            switchTimer->start(connTimeout->value() * 1000);
        setInetStatus(imSwitching);
        info += tr("no ping");
    }
    else
    {
        switchTimer->stop();
        setInetStatus(imConnected);
        int pv = prioTimeout->value();
        if ((switchMode == mAutoSwitch) || (switchMode == mUpperGateway))
        {
            if (indexGateway > 0)
            {
                if ((!prioTimer->isActive()))
                {
                    if (switchMode == mAutoSwitch)
                    {
                        if (prioTimeoutValue * 2 > 21600)
                            prioTimeoutValue = prioTimeout->value();
                        else
                            prioTimeoutValue *= 2;
                        pv = prioTimeoutValue;
                    }
                    prioTimer->start(pv * 60 * 1000); //  in minutes
                }
            }
            else
            {
                prioTimer->stop();
                prioTimeoutValue = prioTimeout->value();
            }
        }
        info += tr("ping:") + QString(" %1 ").arg(res);
        info += tr("ms.");
    }
    gwInfo->setText(info);
}
//------------------------------------------------------------------------------------------------
void Window::switchProc()
{
    QString gw = nextGateway();
    if (!gw.isEmpty())
        switchTo(gw);
    else
        runClearGateway();
}
//------------------------------------------------------------------------------------------------
void Window::prioProc()
{
    indexGateway = -1;
    if (gwList->count())
        switchProc();
}
//------------------------------------------------------------------------------------------------
QString Window::nextGateway()
{
    QString gw;
    int count = gwList->count();
    if (count)
    {
        if ((indexGateway < 0) || (indexGateway == count - 1) || gwListChanged)
        {
            indexGateway = 0;
            gwListChanged = false;
        }
        else
            indexGateway++;
        QListWidgetItem * item = gwList->item(indexGateway);
        if (item)
            gw = item->text();
    }
    int si = switchType->currentIndex();
    forceSwitchButton->setEnabled((si < 2) && (indexGateway > 0));
    return gw;
}
//------------------------------------------------------------------------------------------------
void Window::addHost()
{
    bool changed = false;
    QString ip = gwEdit->text();
    if (isValidIP(ip))
    {
        gwList->addItem(ip);
        gwEdit->clear();
        changed = true;
    }
    canSwitch = ((gwList->count() > 1) && ((switchMode != mNoGateway) && (switchMode != mOnlyGateway)));
    gwList->clearSelection();
    selGw.clear();
    if (changed)
        gatewayListChanged();
}
//------------------------------------------------------------------------------------------------
void Window::delHost()
{
    bool changed = false;
    if (selGw.count())
    {
        QListWidgetItem * item = gwList->takeItem(gwList->row(selGw.at(0)));
        delete item;
        changed = true;
    }
    canSwitch = ((gwList->count() > 1) && ((switchMode != mNoGateway) && (switchMode != mOnlyGateway)));
    gwList->clearSelection();
    selGw.clear();
    if (changed)
        gatewayListChanged();
}
//------------------------------------------------------------------------------------------------
void Window::upHost()
{
    bool changed = false;
    if (selGw.count())
    {
        int row = gwList->row(selGw.at(0));
        if (row > 0)
        {
            QListWidgetItem * item = gwList->takeItem(row);
            gwList->insertItem(row - 1, item);
            changed = true;
        }
    }
    gwList->clearSelection();
    selGw.clear();
    if (changed)
        gatewayListChanged();
}
//------------------------------------------------------------------------------------------------
void Window::downHost()
{
    bool changed = false;
    if (selGw.count())
    {
        int row = gwList->row(selGw.at(0));
        if (row < gwList->count() - 1)
        {
            QListWidgetItem * item = gwList->takeItem(row);
            gwList->insertItem(row + 1, item);
            changed = true;
        }
    }
    gwList->clearSelection();
    selGw.clear();
    if (changed)
        gatewayListChanged();
}
//------------------------------------------------------------------------------------------------
void Window::setVisible(bool visible)
{
    hideAction->setEnabled(visible);
    showAction->setEnabled(!visible);
    QDialog::setVisible(visible);
}
//------------------------------------------------------------------------------------------------
void Window::closeEvent(QCloseEvent *event)
{
    if (trayIcon->isVisible()) {
        closeWanted();
        event->ignore();
    }
}
//------------------------------------------------------------------------------------------------
void Window::setInetStatus(InetStatus is)
{
    if (inetStatus != is)
    {
        inetStatus = is;
        setIcon(is);
    }
}
//------------------------------------------------------------------------------------------------
void Window::setIcon(InetStatus is)
{
    QIcon iconConnected(":/images/computer-network.png");
    QIcon iconSwitching(":/images/computer-exclamation.png");
    QIcon iconDoNothing(":/images/computer-network-gray.png");

    switch (is)
    {
        case imConnected:
            trayIcon->setIcon(iconConnected);
            break;
        case imSwitching:
            trayIcon->setIcon(iconSwitching);
            break;
        case imNoGateway:
            trayIcon->setIcon(iconDoNothing);
            break;
    }
}
//------------------------------------------------------------------------------------------------
void Window::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Trigger:
            if (!isVisible())
                show();
            else
                closeWanted();
            break;
        default:
            break;
    }
}
//------------------------------------------------------------------------------------------------
void Window::createActions()
{
    hideAction = new QAction(tr("Hide"), this);
    hideAction->setEnabled(false);
    connect(hideAction, SIGNAL(triggered()), this, SLOT(closeWanted()));

    showAction = new QAction(tr("Show"), this);
    connect(showAction, SIGNAL(triggered()), this, SLOT(showdlg()));

    quitAction = new QAction(tr("Quit"), this);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(quitApp()));
}
//------------------------------------------------------------------------------------------------
void Window::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(hideAction);
    trayIconMenu->addAction(showAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
}
//------------------------------------------------------------------------------------------------
void Window::quitApp()
{
    if (askOnQuit->isChecked())
    {
        int button = QMessageBox::question(this, "QGateway",
                                           tr("Are you really want to quit?"),
                                           QMessageBox::Ok,
                                           QMessageBox::Cancel);
        if (button != QMessageBox::Ok)
            return;
    }
    writeSettings();
    checkStartup();
    qApp->quit();
}
//------------------------------------------------------------------------------------------------
void Window::about()
{
    QMessageBox::about(this, "QGateway",
                       tr("This program allows to switch gateway for keeping internet connection alive.")
                        + "\n\n"
                     + tr("Author: Roman Ustyugov, 2012"));
}
//------------------------------------------------------------------------------------------------
void Window::gatewayListChanged()
{
    gwListChanged = true;
    int current = switchType->currentIndex();
    bool needToSwitch = false;

    QString cgw;
    if (current > 3)
        cgw = switchType->currentText();

    //  avoid redundant calling slot
    disconnect(switchType, SIGNAL(currentIndexChanged(int)),
            this, SLOT(switchTypeChanged(int)));
    //  remove gateways from combobox
    int c = switchType->count();
    for (int i = 4; i < c; i++)
        switchType->removeItem(4);
    //  connect again
    connect(switchType, SIGNAL(currentIndexChanged(int)),
            this, SLOT(switchTypeChanged(int)));

    QStringList gateways;

    int foundIndex = -1;
    QListWidgetItem * item;
    for (int i = 0; i < gwList->count(); i++)
    {
        item = gwList->item(i);
        if (item)
        {
            if (item->text() == cgw)
                foundIndex = i;
            gateways << item->text();
        }
    }
    if (gateways.count())
        switchType->addItems(gateways);
    if (current >= switchType->count())
    {
        current = 0;
        needToSwitch = true;
    }
    if (!cgw.isEmpty())
    {
        if (foundIndex < 0)
            switchType->setCurrentIndex(0);
        else
            switchType->setCurrentIndex(foundIndex + 4);
    }
    else
        switchType->setCurrentIndex(current);

    applySwitchMode();
    settingsChanged();
}
//------------------------------------------------------------------------------------------------
void Window::settingsChanged()
{
    settChanged = true;
}
//------------------------------------------------------------------------------------------------
void Window::textEdited(const QString &)
{
    settingsChanged();
}
//------------------------------------------------------------------------------------------------
void Window::spinChanged(int)
{
    settingsChanged();
}
//------------------------------------------------------------------------------------------------
void Window::checkboxChanged(int)
{
    settingsChanged();
}

