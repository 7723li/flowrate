QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
    $$PWD/libs/AVTCamera/include/ \
    $$PWD/libs/AVTCamera/include/ \
    $$PWD/libs/AVTCamera/include/VimbaC/ \
    $$PWD/libs/AVTCamera/include/VimbaCPP/ \
    $$PWD/libs/daheng/include/ \
    $$PWD/libs/HALCON/include/ \
    $$PWD/libs/HALCON/include/halconcpp/ \
    $$PWD/libs/opencv/include/ \
    $$PWD/libs/opencv/include/opencv2/ \
    $$PWD/libs/ffmpeg/include/ \

SOURCES += \
    $$PWD/VideoAnalyser.cpp \
    $$PWD/main.cpp \
    $$PWD/widget.cpp \
    $$PWD/flowrate.cpp \
    $$PWD/CameraParamWidget.cpp \

HEADERS += \
    $$PWD/VideoAnalyser.h \
    $$PWD/widget.h \
    $$PWD/flowrate.h \
    $$PWD/CameraParamWidget.h \
    $$PWD/CameraObjectParamProxy.h \

FORMS += \
    $$PWD/CameraParamWidget.ui \
    $$PWD/widget.ui \
    $$PWD/VideoAnalyser.ui \

LIBS += \
    $$PWD/libs/AVTCamera/lib/*.lib \
    $$PWD/libs/daheng/lib/*.lib \
    $$PWD/libs/HALCON/lib/*.lib \
    $$PWD/libs/opencv/lib/*.lib \
    $$PWD/libs/ffmpeg/lib/*.lib \

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
