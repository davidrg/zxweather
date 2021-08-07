CONFIG += qt
QT     += core  gui 


TARGET = ecpg
TEMPLATE = app

SOURCES += main.cpp

win32 {
    ECPG_BIN = $$PWD/../../tools/ecpg-9.1-win32/ecpg.exe
}
unix|mac {
    ECPG_BIN = $$system(which ecpg)
}

isEmpty(ECPG_BIN) {
	error("ECPG not found!")
}

win32 { # Use bundled postgres libraries
	LIBS += -L$$PWD/../../lib/libecpg-9.1-win32 -lecpg
	LIBS += -L$$PWD/../../lib/libpq-9.1-win32 -lpq
	#INCLUDEPATH += $$PWD/../../lib/libecpg-9.1-win32/include
	INCLUDEPATH += lib/libecpg-9.1-win32/include
}
unix { # Assumes libecpg and libpq have been installed in /usr/lib
	LIBS += -L/usr/lib -lpq
	LIBS += -L/usr/lib -lecpg
	INCLUDEPATH += /usr/include/postgresql
}

