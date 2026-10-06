QT       += core gui widgets

TARGET = Personnel
TEMPLATE = app

SOURCES += \
    main.cpp \
    gpersonnel.cpp \
    personnel.cpp \
    vehicule.cpp \
    personneldialog.cpp \
    vehiculedialog.cpp \
    statsdialog.cpp \
    pdfexport.cpp

HEADERS += \
    gpersonnel.h \
    personnel.h \
    vehicule.h \
    personneldialog.h \
    vehiculedialog.h \
    statsdialog.h \
    pdfexport.h \
    styleutils.h

# Un seul fichier .ui
FORMS += \
    gpersonnel.ui

RESOURCES += \
    resources.qrc
