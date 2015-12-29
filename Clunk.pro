TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CXXFLAGS_RELEASE -= -mtune=generic
QMAKE_CXXFLAGS_RELEASE += -mtune=native

INCLUDEPATH += .
INCLUDEPATH += src

unix:QMAKE_CXXFLAGS += -std=c++11
unix::LIBS += -lpthread

CONFIG(release, debug|release) {
  message(Release build!)
  DEFINES += NDEBUG
}

# deploy epd files with each build
include(epd.pri)

# add GITREV to QMAKE_CXXFLAGS
include(gitrev.pri)

SOURCES += \
    src/Clunk.cpp \
    src/HashTable.cpp \
    src/Stats.cpp \
    src/main.cpp \
    src/senjo/BackgroundCommand.cpp \
    src/senjo/ChessEngine.cpp \
    src/senjo/EngineOption.cpp \
    src/senjo/MoveFinder.cpp \
    src/senjo/Output.cpp \
    src/senjo/Threading.cpp \
    src/senjo/UCIAdapter.cpp

HEADERS += \
    src/Clunk.h \
    src/Move.h \
    src/Stats.h \
    src/Types.h \
    src/HashTable.h \
    src/senjo/BackgroundCommand.h \
    src/senjo/ChessEngine.h \
    src/senjo/ChessMove.h \
    src/senjo/EngineOption.h \
    src/senjo/MoveFinder.h \
    src/senjo/Output.h \
    src/senjo/Platform.h \
    src/senjo/Square.h \
    src/senjo/Threading.h \
    src/senjo/UCIAdapter.h \
    src/Defs.h

DISTFILES += \
    epd/*.epd
