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

#ifndef WINDOW_H
#define WINDOW_H

#include <QSystemTrayIcon>
#include <QDialog>

QT_BEGIN_NAMESPACE
class QAction;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QMenu;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QTabWidget;
class QListWidget;
class QListWidgetItem;
class QTimer;
class QErrorMessage;
class QLabel;
QT_END_NAMESPACE

enum Mode {
    mAutoSwitch,
    mUpperGateway,
    mAnyGateway,
    mOnlyGateway,
    mNoGateway
};

enum InetStatus {
    imConnected,
    imSwitching,
    imNoGateway
};

class Window : public QDialog
{
    Q_OBJECT

public:
              Window();

        void  setVisible(bool visible);

protected:
        void  closeEvent(QCloseEvent *event);

private slots:
        void  iconActivated(QSystemTrayIcon::ActivationReason reason);
        void  messageClicked();
        void  addHost();
        void  delHost();
        void  upHost();
        void  downHost();
        void  switchTypeChanged(int index);
        void  pingProc();
        void  switchProc();
        void  prioProc();
        void  gatewaySelectionChanged();
        void  gwTextChanged(const QString & text);
        void  showdlg();
        void  quitApp();
        void  about();
        void  closeWanted();
        void  forceSwitch();
        void  startupChecked(int);
        void  textEdited(const QString &);
        void  settingsChanged();
        void  spinChanged(int);
        void  checkboxChanged(int);

private:
        void  createTabs();
        void  addOptionsTab();
        void  addGatewaysTab();
        void  checkButtons();
        bool  isValidIP(const QString & ip);
        void  setIcon(InetStatus is);
        void  setInetStatus(InetStatus is);
     QString  nextGateway();
        void  checkStartup();
        void  readSettings();
        void  writeSettings();
     QString  iniFileName();
        void  gatewayListChanged();
        void  runClearGateway();
        void  runAddGateway(QString gw);
        void  applySwitchMode();
        void  applySettings();
        void  switchTo(QString gw);
        void  createActions();
        void  createTrayIcon();

                  QTabWidget *tabs;
                   QLineEdit *gwEdit;
                 QListWidget *gwList;
                 QPushButton *forceSwitchButton;
                 QPushButton *addGw;
                 QPushButton *upGw;
                 QPushButton *downGw;
                 QPushButton *delGw;
                 QPushButton *quitButton;
                 QPushButton *closeButton;
                 QPushButton *aboutButton;
                   QComboBox *switchType;
                   QLineEdit *pingHostEdit;
                     QString  pingHost;
                    QSpinBox *connTimeout;
                    QSpinBox *prioTimeout;
                    QSpinBox *pingTimeout;
                         int  prioTimeoutValue;
                      QTimer *pingTimer;
                      QTimer *switchTimer;
                      QTimer *prioTimer;
                     QString  currentGw;
                     QString  routeError;
               QErrorMessage *errorMessage;
                        bool  canSwitch;
                        Mode  switchMode;
                  InetStatus  inetStatus;
    QList<QListWidgetItem *>  selGw;
                         int  indexGateway;
                   QCheckBox *runOnStartup;
                   QCheckBox *showBalloons;
                   QCheckBox *askOnQuit;
                        bool  gwListChanged;
                      QLabel *gwInfo;
                        bool  startupChanged;
                        bool  settChanged;

                     QAction *hideAction;
                     QAction *showAction;
                     QAction *quitAction;

             QSystemTrayIcon *trayIcon;
                       QMenu *trayIconMenu;
};

#endif
