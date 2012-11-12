HEADERS      = window.h \
               ping.h
SOURCES      = main.cpp \
               window.cpp
RESOURCES    = qgateway.qrc
TRANSLATIONS = qgateway_ru.ts
win32:LIBS  += -lws2_32

# supporting native icmp functions on Windows
CONFIG += ms_ping

ms_ping {
    DEFINES += HAVE_MS_PING
    HEADERS += ms_ping.h
    SOURCES += ms_ping.cpp
    win32:LIBS += -lIphlpapi
}

