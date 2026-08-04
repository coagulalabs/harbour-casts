TEMPLATE = app
TARGET = tst_feedparser
CONFIG += console testcase c++14
CONFIG -= app_bundle
QT += core testlib
QT -= gui

INCLUDEPATH += $$PWD/../src

SOURCES += \
    tst_feedparser.cpp \
    ../src/feedparser.cpp

HEADERS += \
    ../src/feedparser.h

TESTDATA += fixtures/*
