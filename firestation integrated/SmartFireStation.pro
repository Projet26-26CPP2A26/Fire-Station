QT       += core gui widgets sql

CONFIG   += c++17

TARGET    = SmartFireStation
TEMPLATE  = app

SOURCES += \
    main.cpp \
    gsmartfirestation.cpp \
    personnel.cpp

HEADERS += \
    gsmartfirestation.h \
    personnel.h

FORMS += gsmartfirestation.ui

RESOURCES += resources.qrc
