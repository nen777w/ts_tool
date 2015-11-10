TARGET = ts_tool
CONFIG += core xml console
TEMPLATE = app

#-------------------------------------------------------------------------------------
GENF_ROOT   = _output
BIN_OUTPUT  = $${GENF_ROOT}/_bin
#-------------------------------------------------------------------------------------

CONFIG(release, debug|release) {
    BUILD_TYPE = release
} else {
    BUILD_TYPE = debug
}

DESTDIR     = $${BIN_OUTPUT}/$${BUILD_TYPE}
OBJECTS_DIR = $${GENF_ROOT}/$${TARGET}/$${BUILD_TYPE}/_build
MOC_DIR     = $${GENF_ROOT}/$${TARGET}/$${BUILD_TYPE}/_moc
UI_DIR      = $${GENF_ROOT}/$${TARGET}/$${BUILD_TYPE}/_ui
RCC_DIR     = $${GENF_ROOT}/$${TARGET}/$${BUILD_TYPE}/_rc

##-------------------------------------------------------------------------------------

SOURCES += \
    ./main.cpp \
    ./ts_model.cpp


HEADERS += \
    ./ts_model.h \
    ./efl_hash.h

win32-g++{
    contains(QMAKE_HOST.arch, x86_64) { #x64
        DEFINES += MINGW_X64
    } else { #x32
        DEFINES += MINGW_X32
    }

    CONFIG(release, debug|release) {
        #release
        QMAKE_CXXFLAGS += -std=c++0x -O2 -Os -msse2 -ffp-contract=fast -fpic
    }
    else {
        #debug
        DEFINES += _DEBUG
        QMAKE_CXXFLAGS += -std=c++0x -O0 -g3 -msse2 -fpic
    }
}
