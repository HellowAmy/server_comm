TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp \
        ux_epoll.cpp \
        ux_server.cpp

HEADERS += \
    log_show.h \
    ux_epoll.h \
    ux_protocol.h \
    ux_server.h
