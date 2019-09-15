#--------------------------------------------------------------------
# global var
#--------------------------------------------------------------------

SPARK_DIR = ../../../spark
THIRDPARTY_DIR = ../../../thirdparty

#--------------------------------------------------------------------
# target
#--------------------------------------------------------------------

TEMPLATE = lib
CONFIG -= qt
CONFIG += staticlib
CONFIG += c++11
CONFIG -= debug_and_release
CONFIG -= debug_and_release_target

#--------------------------------------------------------------------
# output directory
#--------------------------------------------------------------------

CONFIG(debug,debug|release){
    DESTDIR = $$PWD/../../../demos/bin
} else {
    DESTDIR = $$PWD/../../../demos/bin
}
QMAKE_CLEAN += $$DESTDIR/$$TARGET

#--------------------------------------------------------------------
# compilation flags
#--------------------------------------------------------------------

unix:!macx: QMAKE_CXXFLAGS_WARN_ON -= -Wall
unix:!macx: QMAKE_CFLAGS_WARN_ON -= -Wall
unix:!macx: QMAKE_CXXFLAGS += -Wall
#unix:!macx: QMAKE_CXXFLAGS += -Wno-comment
#unix:!macx: QMAKE_CXXFLAGS += -Wno-ignored-qualifiers
unix:!macx: QMAKE_CXXFLAGS += -Wno-unused-parameter
unix:!macx: QMAKE_CXXFLAGS += -std=c++11
#unix:!macx: QMAKE_CXXFLAGS += -fpermissive
#unix:!macx: QMAKE_CXXFLAGS += -Wno-unused-function
#unix:!macx: QMAKE_CXXFLAGS += -Wno-reorder
#unix:!macx: QMAKE_CXXFLAGS += -Wfatal-errors
#unix:!macx: QMAKE_CXXFLAGS += -m32

CONFIG(debug,debug|release) {
    QMAKE_CXXFLAGS_RELEASE += -O0
} else {
    QMAKE_CXXFLAGS_RELEASE += -O3
}

#--------------------------------------------------------------------
# pre-processor definitions
#--------------------------------------------------------------------

CONFIG(debug,debug|release) {
    #debug
    DEFINES +=  \
        #SPK_GL_NO_EXT \

} else {
    # release
    DEFINES +=  \
}

#--------------------------------------------------------------------
# libraries includes
#--------------------------------------------------------------------

INCLUDEPATH += $${SPARK_DIR}/include
win32:INCLUDEPATH += $${THIRDPARTY_DIR}/glew-2.1.0/include


#--------------------------------------------------------------------
# project files
#--------------------------------------------------------------------

HEADERS += \
    ../../../rendering/SPARK_OpenGL/SPARK_GL.h \
    ../../../rendering/SPARK_OpenGL/SPK_GL_Buffer.h \
    ../../../rendering/SPARK_OpenGL/SPK_GL_DEF.h \
    ../../../rendering/SPARK_OpenGL/SPK_GL_LineRenderer.h \
    ../../../rendering/SPARK_OpenGL/SPK_GL_LineTrailRenderer.h \
    ../../../rendering/SPARK_OpenGL/SPK_GL_PointRenderer.h \
    ../../../rendering/SPARK_OpenGL/SPK_GL_QuadRenderer.h \
    ../../../rendering/SPARK_OpenGL/SPK_GL_Renderer.h


SOURCES += \
    ../../../rendering/SPARK_OpenGL/SPK_GL_Buffer.cpp \
    ../../../rendering/SPARK_OpenGL/SPK_GL_LineRenderer.cpp \
    ../../../rendering/SPARK_OpenGL/SPK_GL_LineTrailRenderer.cpp \
    ../../../rendering/SPARK_OpenGL/SPK_GL_PointRenderer.cpp \
    ../../../rendering/SPARK_OpenGL/SPK_GL_QuadRenderer.cpp \
    ../../../rendering/SPARK_OpenGL/SPK_GL_Renderer.cpp








