QT       += core gui widgets

TARGET = Vehicules
TEMPLATE = app

SOURCES += \
    main.cpp \
    gvehicule.cpp \
    personnel.cpp \
    vehicule.cpp \
    personneldialog.cpp \
    vehiculedialog.cpp \
    statsdialog.cpp \
    pdfexport.cpp

HEADERS += \
    gvehicule.h \
    personnel.h \
    vehicule.h \
    personneldialog.h \
    vehiculedialog.h \
    statsdialog.h \
    pdfexport.h \
    styleutils.h

# Un seul fichier .ui
FORMS += \
    gvehicule.ui

RESOURCES += \
    resources.qrc
