QT       += core gui widgets sql

CONFIG   += c++17

TARGET    = SmartFireStation
TEMPLATE  = app

SOURCES += \
    main.cpp \
    gsmartfirestation.cpp

HEADERS += \
    gsmartfirestation.h

FORMS += gsmartfirestation.ui

RESOURCES += resources.qrc
