QT       += core gui widgets

DEFINES += STANDALONE_PERSONNEL

TARGET = Personnel
TEMPLATE = app

SOURCES += \
    main.cpp \
    gsmartfirestation.cpp \
    gsmartfirestation_personnel.cpp \
    gsmartfirestation_vehicules.cpp \
    personnel.cpp \
    vehicule.cpp \
    personneldialog.cpp \
    vehiculedialog.cpp \
    statsdialog.cpp \
    pdfexport.cpp \
    piechartwidget.cpp

HEADERS += \
    gsmartfirestation.h \
    personnel.h \
    vehicule.h \
    personneldialog.h \
    vehiculedialog.h \
    statsdialog.h \
    pdfexport.h \
    styleutils.h \
    piechartwidget.h

FORMS += gsmartfirestation.ui

RESOURCES += resources.qrc
