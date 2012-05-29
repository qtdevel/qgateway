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

#ifdef Q_OS_WIN
#include "winsock2.h"
#include "ws2tcpip.h"
#include "windows.h"
#endif

#include "window.h"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(qgateway);

    QApplication app(argc, argv);

    //  loading language translation
    QString locale = QLocale::system().name();
    QTranslator translator;
    translator.load(QCoreApplication::applicationDirPath() + QDir::separator() + "qgateway_" + locale);
    app.installTranslator(&translator);

    //  checking if application has only one instance
    QSharedMemory sharedMemory("QGateway");
    bool oneInstance = (sharedMemory.create(1) && 
                        (sharedMemory.error() != QSharedMemory::AlreadyExists));
    if (!oneInstance)
    {
        QMessageBox::information(0, "QGateway",
                              QObject::tr("Application is already running!"));
        return 0;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(0, "QGateway",
                              QObject::tr("I couldn't detect any system tray "
                                          "on this system."));
        return 1;
    }

    QApplication::setQuitOnLastWindowClosed(false);

#ifdef Q_OS_WIN
    WORD    wVersionRequested;
    WSADATA wsaData;
    int     err;

    wVersionRequested = MAKEWORD( 2, 2 );

    err = WSAStartup( wVersionRequested, &wsaData );

    if( err != 0 )
    {
            /* Tell the user that we could not find a usable */
            /* WinSock DLL.                                  */
            QMessageBox::critical(0, "QGateway",
                              QObject::tr("I couldn't detect required Windows socket library "
                                          "on this system."));
            return err;
    }

    /* Confirm that the WinSock DLL supports 2.2.*/
    /* Note that if the DLL supports versions greater    */
    /* than 2.2 in addition to 2.2, it will still return */
    /* 2.2 in wVersion since that is the version we      */
    /* requested.                                        */

    if( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 2 )
    {
            /* Tell the user that we could not find a usable */
            /* WinSock DLL.                                  */
        QMessageBox::critical(0, "QGateway",
                              QObject::tr("I couldn't detect required Windows socket library "
                                          "on this system."));
        WSACleanup();
        return 1;
    }

    /* The WinSock DLL is acceptable. Proceed. */
#endif

    Window window;
    return app.exec();

#ifdef Q_OS_WIN
    WSACleanup();
#endif
}
