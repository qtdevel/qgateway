HEADERS      = window.h \
               ping.h
SOURCES      = main.cpp \
               window.cpp
RESOURCES    = qgateway.qrc
TRANSLATIONS = qgateway_ru.ts
win32:LIBS  += -lws2_32
