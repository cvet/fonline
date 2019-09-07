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
unix:!macx: QMAKE_CXXFLAGS += -Wextra
#unix:!macx: QMAKE_CXXFLAGS += -Wno-comment
#unix:!macx: QMAKE_CXXFLAGS += -Wno-ignored-qualifiers
unix:!macx: QMAKE_CXXFLAGS += -Wno-unused-parameter
unix:!macx: QMAKE_CXXFLAGS += -std=c++11
#unix:!macx: QMAKE_CXXFLAGS += -fpermissive
#unix:!macx: QMAKE_CXXFLAGS += -Wno-unused-function
#unix:!macx: QMAKE_CXXFLAGS += -Wno-reorder
#unix:!macx: QMAKE_CXXFLAGS += -Wfatal-errors
unix:!macx: QMAKE_CXXFLAGS += -Wsuggest-override
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
        SPK_DEBUG \
        #SPK_NO_LOG \
        #SPK_TRACE_MEMORY \
        #SPK_NO_XML

} else {
    # release
    DEFINES +=  \
}

#--------------------------------------------------------------------
# libraries includes
#--------------------------------------------------------------------

INCLUDEPATH += $${SPARK_DIR}/include
INCLUDEPATH += $${THIRDPARTY_DIR}/PugiXml

#--------------------------------------------------------------------
# project files
#--------------------------------------------------------------------

HEADERS += \
    $${SPARK_DIR}/include/Core/IO/SPK_IO_Attribute.h \
    $${SPARK_DIR}/include/Core/IO/SPK_IO_Buffer.h \
    $${SPARK_DIR}/include/Core/IO/SPK_IO_Descriptor.h \
    $${SPARK_DIR}/include/Core/IO/SPK_IO_Loader.h \
    $${SPARK_DIR}/include/Core/IO/SPK_IO_Manager.h \
    $${SPARK_DIR}/include/Core/IO/SPK_IO_Saver.h \
    $${SPARK_DIR}/include/Core/SPK_Action.h \
    $${SPARK_DIR}/include/Core/SPK_ArrayData.h \
    $${SPARK_DIR}/include/Core/SPK_Color.h \
    $${SPARK_DIR}/include/Core/SPK_DataHandler.h \
    $${SPARK_DIR}/include/Core/SPK_DataSet.h \
    $${SPARK_DIR}/include/Core/SPK_DEF.h \
    $${SPARK_DIR}/include/Core/SPK_Emitter.h \
    $${SPARK_DIR}/include/Core/SPK_Enum.h \
    $${SPARK_DIR}/include/Core/SPK_Group.h \
    $${SPARK_DIR}/include/Core/SPK_Interpolator.h \
    $${SPARK_DIR}/include/Core/SPK_Iterator.h \
    $${SPARK_DIR}/include/Core/SPK_Logger.h \
    $${SPARK_DIR}/include/Core/SPK_MemoryTracer.h \
    $${SPARK_DIR}/include/Core/SPK_Modifier.h \
    $${SPARK_DIR}/include/Core/SPK_Object.h \
    $${SPARK_DIR}/include/Core/SPK_Octree.h \
    $${SPARK_DIR}/include/Core/SPK_Particle.h \
    $${SPARK_DIR}/include/Core/SPK_Reference.h \
    $${SPARK_DIR}/include/Core/SPK_RenderBuffer.h \
    $${SPARK_DIR}/include/Core/SPK_Renderer.h \
    $${SPARK_DIR}/include/Core/SPK_System.h \
    $${SPARK_DIR}/include/Core/SPK_Transform.h \
    $${SPARK_DIR}/include/Core/SPK_Transformable.h \
    $${SPARK_DIR}/include/Core/SPK_Vector3D.h \
    $${SPARK_DIR}/include/Core/SPK_Zone.h \
    $${SPARK_DIR}/include/Core/SPK_ZonedModifier.h \
    $${SPARK_DIR}/include/Extensions/Actions/SPK_ActionSet.h \
    $${SPARK_DIR}/include/Extensions/Actions/SPK_SpawnParticlesAction.h \
    $${SPARK_DIR}/include/Extensions/Emitters/SPK_NormalEmitter.h \
    $${SPARK_DIR}/include/Extensions/Emitters/SPK_RandomEmitter.h \
    $${SPARK_DIR}/include/Extensions/Emitters/SPK_SphericEmitter.h \
    $${SPARK_DIR}/include/Extensions/Emitters/SPK_StaticEmitter.h \
    $${SPARK_DIR}/include/Extensions/Emitters/SPK_StraightEmitter.h \
    $${SPARK_DIR}/include/Extensions/Interpolators/SPK_DefaultInitializer.h \
    $${SPARK_DIR}/include/Extensions/Interpolators/SPK_GraphInterpolator.h \
    $${SPARK_DIR}/include/Extensions/Interpolators/SPK_RandomInitializer.h \
    $${SPARK_DIR}/include/Extensions/Interpolators/SPK_RandomInterpolator.h \
    $${SPARK_DIR}/include/Extensions/Interpolators/SPK_SimpleInterpolator.h \
    $${SPARK_DIR}/include/Extensions/IOConverters/SPK_IO_SPKLoader.h \
    $${SPARK_DIR}/include/Extensions/IOConverters/SPK_IO_SPKSaver.h \
    $${SPARK_DIR}/include/Extensions/IOConverters/SPK_IO_XMLLoader.h \
    $${SPARK_DIR}/include/Extensions/IOConverters/SPK_IO_XMLSaver.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_BasicModifiers.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_Collider.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_Destroyer.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_EmitterAttacher.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_LinearForce.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_Obstacle.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_PointMass.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_RandomForce.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_Rotator.h \
    $${SPARK_DIR}/include/Extensions/Modifiers/SPK_Vortex.h \
    $${SPARK_DIR}/include/Extensions/Renderers/SPK_LineRenderBehavior.h \
    $${SPARK_DIR}/include/Extensions/Renderers/SPK_Oriented3DRenderBehavior.h \
    $${SPARK_DIR}/include/Extensions/Renderers/SPK_PointRenderBehavior.h \
    $${SPARK_DIR}/include/Extensions/Renderers/SPK_QuadRenderBehavior.h \
    $${SPARK_DIR}/include/Extensions/Zones/SPK_Box.h \
    $${SPARK_DIR}/include/Extensions/Zones/SPK_Cylinder.h \
    $${SPARK_DIR}/include/Extensions/Zones/SPK_Plane.h \
    $${SPARK_DIR}/include/Extensions/Zones/SPK_Point.h \
    $${SPARK_DIR}/include/Extensions/Zones/SPK_Ring.h \
    $${SPARK_DIR}/include/Extensions/Zones/SPK_Sphere.h \
    $${SPARK_DIR}/include/SPARK.h \
    $${SPARK_DIR}/include/SPARK_Core.h \

SOURCES += \
    $${SPARK_DIR}/src/Core/IO/SPK_IO_Buffer.cpp \
    $${SPARK_DIR}/src/Core/IO/SPK_IO_Descriptor.cpp \
    $${SPARK_DIR}/src/Core/IO/SPK_IO_Loader.cpp \
    $${SPARK_DIR}/src/Core/IO/SPK_IO_Manager.cpp \
    $${SPARK_DIR}/src/Core/IO/SPK_IO_Saver.cpp \
    $${SPARK_DIR}/src/Core/SPK_DataSet.cpp \
    $${SPARK_DIR}/src/Core/SPK_DEF.cpp \
    $${SPARK_DIR}/src/Core/SPK_MemoryTracer.cpp \
    $${SPARK_DIR}/src/Core/SPK_Emitter.cpp \
    $${SPARK_DIR}/src/Core/SPK_Group.cpp \
    $${SPARK_DIR}/src/Core/SPK_Logger.cpp \
    $${SPARK_DIR}/src/Core/SPK_Object.cpp \
    $${SPARK_DIR}/src/Core/SPK_Octree.cpp \
    $${SPARK_DIR}/src/Core/SPK_Renderer.cpp \
    $${SPARK_DIR}/src/Core/SPK_System.cpp \
    $${SPARK_DIR}/src/Core/SPK_Transform.cpp \
    $${SPARK_DIR}/src/Core/SPK_Transformable.cpp \
    $${SPARK_DIR}/src/Core/SPK_Zone.cpp \
    $${SPARK_DIR}/src/Core/SPK_ZonedModifier.cpp \
    $${SPARK_DIR}/src/Extensions/Actions/SPK_ActionSet.cpp \
    $${SPARK_DIR}/src/Extensions/Actions/SPK_SpawnParticlesAction.cpp \
    $${SPARK_DIR}/src/Extensions/Emitters/SPK_NormalEmitter.cpp \
    $${SPARK_DIR}/src/Extensions/Emitters/SPK_RandomEmitter.cpp \
    $${SPARK_DIR}/src/Extensions/Emitters/SPK_SphericEmitter.cpp \
    $${SPARK_DIR}/src/Extensions/Emitters/SPK_StraightEmitter.cpp \
    $${SPARK_DIR}/src/Extensions/IOConverters/SPK_IO_SPKLoader.cpp \
    $${SPARK_DIR}/src/Extensions/IOConverters/SPK_IO_SPKSaver.cpp \
    $${SPARK_DIR}/src/Extensions/IOConverters/SPK_IO_XMLLoader.cpp \
    $${SPARK_DIR}/src/Extensions/IOConverters/SPK_IO_XMLSaver.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_BasicModifiers.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_Collider.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_EmitterAttacher.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_LinearForce.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_Obstacle.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_PointMass.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_RandomForce.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_Rotator.cpp \
    $${SPARK_DIR}/src/Extensions/Modifiers/SPK_Vortex.cpp \
    $${SPARK_DIR}/src/Extensions/Renderers/SPK_Oriented3DRenderBehavior.cpp \
    $${SPARK_DIR}/src/Extensions/Renderers/SPK_QuadRenderBehavior.cpp \
    $${SPARK_DIR}/src/Extensions/Zones/SPK_Box.cpp \
    $${SPARK_DIR}/src/Extensions/Zones/SPK_Cylinder.cpp \
    $${SPARK_DIR}/src/Extensions/Zones/SPK_Plane.cpp \
    $${SPARK_DIR}/src/Extensions/Zones/SPK_Ring.cpp \
    $${SPARK_DIR}/src/Extensions/Zones/SPK_Sphere.cpp \


