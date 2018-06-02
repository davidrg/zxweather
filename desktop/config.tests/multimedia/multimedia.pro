CONFIG += qt
QT       += core gui 

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets multimedia multimediawidgets

lessThan(QT_MAJOR_VERSION, 5): QT += phonon

TARGET = multimedia
TEMPLATE = app

SOURCES += main.cpp
