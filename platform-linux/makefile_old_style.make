#first time type -> 'make lib'
#then type 'make app'
#if -> on error (GLES2/gl2.h: No such file or directory
#type 'sudo apt-get install libgles2-mesa-dev'

CC 			=clang
CXX 		=clang++
LOCAL_PATH 	= $(call my-dir)
LIBS 		=-lm -ldl -lX11 -lEGL -lGLESv2 -lstdc++ 
FLAGS   	=-g -Wall -std=c++11 -pthread -Wno-float-equal -Wno-switch-enum -Wno-padded -Wno-format-nonliteral
FLAGS_C  	=-g -Wall
DEF 		= -DUSE_OPENGL_ES -DUSE_DEPRECATED_2_MINOR -DUSE_SQLITE3 -DUSE_VR -D_DEBUG -DLUA_COMPAT_ALL -DLUA_ANSI -DUSE_EDITOR_FEATURES
INCLUDES 	=-I../include/ -I../include/core_mbm -I../third-party/ -I../include/render -I../third-party/stb -I../include/lua-5.4 -I../third-party/lua-5.4.1

ifndef CORE_HOME
    export CORE_HOME=$(LOCAL_PATH)
endif

ifndef SRC
    export SRC=$(CORE_HOME)../src/
endif

ifndef THIRD_PARTY
    export THIRD_PARTY=$(CORE_HOME)../third-party
endif

CORE_OPENGL        =$(SRC)core_mbm
CORE_RENDER        =$(SRC)render

LUA_WRAP         	=$(SRC)lua-wrap
LUA_RENDER_WRAP 	=$(SRC)lua-wrap/render-table
LUA_PHYSICS_TABLE 	=$(SRC)lua-wrap/physics-table
TINY_FIL         	=$(THIRD_PARTY)/tinyfiledialogs
MINIZ             	=$(THIRD_PARTY)/miniz
LODEPNG           	=$(THIRD_PARTY)/lodepng
RESOURCE        	=$(SRC)static-resource
CORE_LUA        	=$(SRC)lua-wrap
TEST_LIB        	=$(SRC)test-lib
STB             	=$(THIRD_PARTY)/stb



# Define vars for library that will be build statically.
LUA_SRC     := $(THIRD_PARTY)/lua-5.4.1
LUA_C_INC   := -I$(LUA_SRC)
LUA_CFLAG   := -O2 -Wall -DLUA_COMPAT_ALL -DLUA_ANSI

#Box2d
BOX2D_SRC       =$(THIRD_PARTY)/box2d-2.3.1/Box2D
BOX2D_INC       =-I$(THIRD_PARTY)/box2d-2.3.1
BOX2D_WRAP      =$(SRC)/box-2d-wrap
BOX2D_CFLAGS    =-O2 -Wall -Wno-strict-aliasing -std=c++11

#sqlite
SQLITE_SRC      =$(THIRD_PARTY)/sqlite3
SQLITE_INC      =-I$(THIRD_PARTY)/sqlite3

#bullet3d
BULLET3D_SRC    =$(THIRD_PARTY)/bullet-2.84
BULLET3D_INC    =-I$(THIRD_PARTY)/bullet-2.84
BULLET3D_WRAP   =$(SRC)/bullet-3d-wrap
BULLET3D_CFLAGS =-O2 -DUSE_PTHREADS -pthread -DSCE_PFX_USE_SIMD_VECTORMATH -Wunused-function -Wunused-variable -Wno-strict-aliasing -Wno-unused-function -Wno-unused-variable

.PHONY: all

all: core_mbm test lua lua-core sqlite3 box-2d box-2d bullet3d mini-mbm
    
core_mbm:
	$(CXX) -c $(CORE_OPENGL)/animation.cpp              	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/audio-interface.cpp        	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/blend.cpp                  	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/camera.cpp                 	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/core-manager.cpp           	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/deprecated.cpp             	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/device.cpp                 	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/dynamic-var.cpp            	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/file-util.cpp              	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/frustum.cpp                	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/gles-debug.cpp             	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/header-mesh.cpp            	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/image-resource.cpp         	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/log-util.cpp               	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/mesh-manager.cpp           	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/miniz/miniz-wrap.cpp       	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/order-render.cpp           	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/physics.cpp                	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/primitives.cpp             	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/renderizable.cpp           	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/scene.cpp                  	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/shader-cfg.cpp             	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/shader.cpp                 	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/shader-var-cfg.cpp         	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/shapes.cpp                 	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/simple-audio.cpp           	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/texture-manager.cpp        	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/time-control.cpp           	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/uber-image.cpp             	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_OPENGL)/util.cpp                   	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/background.cpp             	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/font.cpp                   	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/shape-mesh.cpp             	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/gif-view.cpp               	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/HMD.cpp                    	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/line-mesh.cpp              	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/mesh.cpp                   	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/particle.cpp               	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/render-2-texture.cpp       	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/sprite.cpp                 	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(CORE_RENDER)/texture-view.cpp           	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CC)  -c $(TINY_FIL)/tinyfiledialogs.c             	$(INCLUDES) $(LIB_PATH) $(FLAGS_C) $(DEF)
	$(CC)  -c $(MINIZ)/miniz.c                          	$(INCLUDES) $(LIB_PATH) $(FLAGS_C) $(DEF)
	$(CC)  -c $(MINIZ)/miniz_tdef.c                     	$(INCLUDES) $(LIB_PATH) $(FLAGS_C) $(DEF)
	$(CC)  -c $(MINIZ)/miniz_tinfl.c                    	$(INCLUDES) $(LIB_PATH) $(FLAGS_C) $(DEF)
	$(CC)  -c $(MINIZ)/miniz_zip.c                      	$(INCLUDES) $(LIB_PATH) $(FLAGS_C) $(DEF)
	$(CXX) -c $(LODEPNG)/lodepng.cpp                    	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LODEPNG)/lodepng_util.cpp               	$(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)    
	$(CC)  -c $(STB)/stb.c 		                           	$(INCLUDES) $(LIB_PATH) $(FLAGS_C) $(DEF)
	$(CXX) -c $(BOX2D_WRAP)/box-2d-wrap.cpp             	$(INCLUDES) $(BOX2D_INC) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(BULLET3D_WRAP)/shape-info-bullet-3d.cpp 	$(INCLUDES) $(BULLET3D_INC) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(BULLET3D_WRAP)/physics-bullet-3d-wrap.cpp 	$(INCLUDES) $(BULLET3D_INC) $(LIB_PATH) $(FLAGS) $(DEF)
	ar rcs  $(CORE_HOME)core_mbm.a                 	$(CORE_HOME)*.o
	rm *.o
test:
	$(CXX) -c $(TEST_LIB)/my-scene-test.cpp         $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(TEST_LIB)/main.cpp                  $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) *.o -o libTest.exe                       $(INCLUDES) $(LIB_PATH) $(FLAGS) -g $(DEF) $(LIBS) core_mbm.a
	rm *.o
lua:
	$(CC) -c $(LUA_SRC)/lapi.c      $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lcode.c     $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lctype.c    $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/ldebug.c    $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/ldo.c       $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/ldump.c     $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lfunc.c     $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lgc.c       $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/llex.c      $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lmem.c      $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lobject.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lopcodes.c  $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lparser.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lstate.c    $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lstring.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/ltable.c    $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/ltm.c       $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lundump.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lvm.c       $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lzio.c      $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lauxlib.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lbaselib.c  $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lbitlib.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lcorolib.c  $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/ldblib.c    $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/liolib.c    $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lmathlib.c  $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/loslib.c    $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/lstrlib.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/ltablib.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/loadlib.c   $(LUA_CFLAG) $(LUA_C_INC)
	$(CC) -c $(LUA_SRC)/linit.c     $(LUA_CFLAG) $(LUA_C_INC)
	ar rcs  $(CORE_HOME)lua.a       $(CORE_HOME)*.o
	rm *.o
lua-core:
	$(CXX) -c $(LUA_WRAP)/audio-lua.cpp                         $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/camera-lua.cpp                        $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/common-methods-lua.cpp                $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/current-scene-lua.cpp                 $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/framework-lua.cpp                     $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/manager-lua.cpp                       $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/shader-lua.cpp                        $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/timer-lua.cpp                         $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/user-data-lua.cpp                     $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/vec2-lua.cpp                          $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/vec3-lua.cpp                          $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_WRAP)/check-user-type-lua.cpp               $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/animation-lua.cpp              $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/background-lua.cpp             $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/font-lua.cpp                   $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/shape-lua.cpp                  $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/gif-view-lua.cpp               $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/line-mesh-lua.cpp              $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/mesh-debug-lua.cpp             $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/mesh-lua.cpp                   $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/particle-lua.cpp               $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/render-2-texture-lua.cpp       $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/sprite-lua.cpp                 $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/texture-view-lua.cpp           $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_RENDER_WRAP)/vr-lua.cpp                     $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_PHYSICS_TABLE)/physics-box2d-joint-lua.cpp  $(INCLUDES) $(BOX2D_INC) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_PHYSICS_TABLE)/physics-box-2d-lua.cpp       $(INCLUDES) $(BOX2D_INC) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) -c $(LUA_PHYSICS_TABLE)/physics-bullet-3d-lua.cpp    $(INCLUDES) $(BULLET3D_INC) $(LIB_PATH) $(FLAGS) $(DEF)
	ar rcs  $(CORE_HOME)lua-core.a         						$(CORE_HOME)*.o
	rm *.o
box-2d:
	$(CXX) -c $(BOX2D_SRC)/Collision/b2BroadPhase.cpp                      $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/b2CollideCircle.cpp                   $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/b2CollideEdge.cpp                     $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/b2CollidePolygon.cpp                  $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/b2Collision.cpp                       $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/b2Distance.cpp                        $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/b2DynamicTree.cpp                     $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/b2TimeOfImpact.cpp                    $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/Shapes/b2ChainShape.cpp               $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/Shapes/b2CircleShape.cpp              $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/Shapes/b2EdgeShape.cpp                $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Collision/Shapes/b2PolygonShape.cpp             $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Common/b2BlockAllocator.cpp                     $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Common/b2Draw.cpp                               $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Common/b2Math.cpp                               $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Common/b2Settings.cpp                           $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Common/b2StackAllocator.cpp                     $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Common/b2Timer.cpp                              $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/b2Body.cpp                             $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/b2ContactManager.cpp                   $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/b2Fixture.cpp                          $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/b2Island.cpp                           $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/b2World.cpp                            $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/b2WorldCallbacks.cpp                   $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2ChainAndCircleContact.cpp   $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2ChainAndPolygonContact.cpp  $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2CircleContact.cpp           $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2Contact.cpp                 $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2ContactSolver.cpp           $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2EdgeAndCircleContact.cpp    $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2EdgeAndPolygonContact.cpp   $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2PolygonAndCircleContact.cpp $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Contacts/b2PolygonContact.cpp          $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2DistanceJoint.cpp             $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2FrictionJoint.cpp             $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2GearJoint.cpp                 $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2Joint.cpp                     $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2MotorJoint.cpp                $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2MouseJoint.cpp                $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2PrismaticJoint.cpp            $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2PulleyJoint.cpp               $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2RevoluteJoint.cpp             $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2RopeJoint.cpp                 $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2WeldJoint.cpp                 $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Dynamics/Joints/b2WheelJoint.cpp                $(BOX2D_CFLAGS) $(BOX2D_INC)
	$(CXX) -c $(BOX2D_SRC)/Rope/b2Rope.cpp                                 $(BOX2D_CFLAGS) $(BOX2D_INC)
	ar rcs  $(CORE_HOME)box-2d.a                                           $(CORE_HOME)*.o
	rm *.o
sqlite3:
	$(CC) -c $(SQLITE_SRC)/sqlite3.c        $(SQLITE_INC)
	$(CC) -c $(SQLITE_SRC)/lsqlite3.c       $(SQLITE_INC) $(LUA_C_INC)
	ar rcs  $(CORE_HOME)sqlite3.a           $(CORE_HOME)*.o
	rm *.o
bullet3d:
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btAxisSweep3.cpp                       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btBroadphaseProxy.cpp                  $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btCollisionAlgorithm.cpp               $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btDbvt.cpp                             $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btDbvtBroadphase.cpp                   $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btDispatcher.cpp                       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btMultiSapBroadphase.cpp               $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btOverlappingPairCache.cpp             $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btQuantizedBvh.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/BroadphaseCollision/btSimpleBroadphase.cpp                 $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btActivatingCollisionAlgorithm.cpp       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.cpp       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btBoxBoxCollisionAlgorithm.cpp           $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btBoxBoxDetector.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btCollisionDispatcher.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btCollisionObject.cpp                    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btCollisionWorld.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btCollisionWorldImporter.cpp             $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btCompoundCollisionAlgorithm.cpp         $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btCompoundCompoundCollisionAlgorithm.cpp $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btConvex2dConvex2dAlgorithm.cpp          $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btConvexConcaveCollisionAlgorithm.cpp    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btConvexConvexAlgorithm.cpp              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btConvexPlaneCollisionAlgorithm.cpp      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.cpp      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btEmptyCollisionAlgorithm.cpp            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btGhostObject.cpp                        $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btHashedSimplePairCache.cpp              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btInternalEdgeUtility.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btManifoldResult.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btSimulationIslandManager.cpp            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btSphereBoxCollisionAlgorithm.cpp        $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btSphereSphereCollisionAlgorithm.cpp     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btSphereTriangleCollisionAlgorithm.cpp   $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/btUnionFind.cpp                          $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionDispatch/SphereTriangleDetector.cpp               $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btBox2dShape.cpp                           $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btBoxShape.cpp                             $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btBvhTriangleMeshShape.cpp                 $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btCapsuleShape.cpp                         $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btCollisionShape.cpp                       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btCompoundShape.cpp                        $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConcaveShape.cpp                         $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConeShape.cpp                            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConvex2dShape.cpp                        $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConvexHullShape.cpp                      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConvexInternalShape.cpp                  $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConvexPointCloudShape.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConvexPolyhedron.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConvexShape.cpp                          $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btConvexTriangleMeshShape.cpp              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btCylinderShape.cpp                        $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btEmptyShape.cpp                           $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.cpp              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btMinkowskiSumShape.cpp                    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btMultimaterialTriangleMeshShape.cpp       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btMultiSphereShape.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btOptimizedBvh.cpp                         $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btPolyhedralConvexShape.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.cpp           $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btShapeHull.cpp                            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btSphereShape.cpp                          $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btStaticPlaneShape.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btStridingMeshInterface.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btTetrahedronShape.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btTriangleBuffer.cpp                       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btTriangleCallback.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btTriangleIndexVertexArray.cpp             $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btTriangleIndexVertexMaterialArray.cpp     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btTriangleMesh.cpp                         $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btTriangleMeshShape.cpp                    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/CollisionShapes/btUniformScalingShape.cpp                  $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/btContactProcessing.cpp                            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/btGenericPoolAllocator.cpp                         $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/btGImpactBvh.cpp                                   $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/btGImpactCollisionAlgorithm.cpp                    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/btGImpactQuantizedBvh.cpp                          $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/btGImpactShape.cpp                                 $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/btTriangleShapeEx.cpp                              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/gim_box_set.cpp                                    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/gim_contact.cpp                                    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/gim_memory.cpp                                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/Gimpact/gim_tri_collision.cpp                              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btContinuousConvexCollision.cpp       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btConvexCast.cpp                      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btGjkConvexCast.cpp                   $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btGjkEpa2.cpp                         $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.cpp    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btGjkPairDetector.cpp                 $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btMinkowskiPenetrationDepthSolver.cpp $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btPersistentManifold.cpp              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btPolyhedralContactClipping.cpp       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btRaycastCallback.cpp                 $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btSubSimplexConvexCast.cpp            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.cpp            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Character/btKinematicCharacterController.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btConeTwistConstraint.cpp                  $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btContactConstraint.cpp                    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btFixedConstraint.cpp                      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btGearConstraint.cpp                       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btGeneric6DofConstraint.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btGeneric6DofSpring2Constraint.cpp         $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btGeneric6DofSpringConstraint.cpp          $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btHinge2Constraint.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btHingeConstraint.cpp                      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btNNCGConstraintSolver.cpp                 $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btPoint2PointConstraint.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.cpp    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btSliderConstraint.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btSolve2LinearConstraint.cpp               $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btTypedConstraint.cpp                      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/ConstraintSolver/btUniversalConstraint.cpp                  $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Dynamics/btDiscreteDynamicsWorld.cpp                        $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Dynamics/btRigidBody.cpp                                    $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Dynamics/btSimpleDynamicsWorld.cpp                          $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Featherstone/btMultiBody.cpp                                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Featherstone/btMultiBodyConstraint.cpp                      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Featherstone/btMultiBodyConstraintSolver.cpp                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Featherstone/btMultiBodyDynamicsWorld.cpp                   $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Featherstone/btMultiBodyJointLimitConstraint.cpp            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Featherstone/btMultiBodyJointMotor.cpp                      $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Featherstone/btMultiBodyPoint2Point.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/MLCPSolvers/btDantzigLCP.cpp                                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/MLCPSolvers/btLemkeAlgorithm.cpp                            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/MLCPSolvers/btMLCPSolver.cpp                                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Vehicle/btRaycastVehicle.cpp                                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletDynamics/Vehicle/btWheelInfo.cpp                                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletSoftBody/btDefaultSoftBodySolver.cpp                                 $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletSoftBody/btSoftBody.cpp                                              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletSoftBody/btSoftBodyConcaveCollisionAlgorithm.cpp                     $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletSoftBody/btSoftBodyHelpers.cpp                                       $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.cpp               $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletSoftBody/btSoftRigidCollisionAlgorithm.cpp                           $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletSoftBody/btSoftRigidDynamicsWorld.cpp                                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/BulletSoftBody/btSoftSoftCollisionAlgorithm.cpp                            $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/LinearMath/btAlignedAllocator.cpp                                          $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/LinearMath/btConvexHull.cpp                                                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/LinearMath/btConvexHullComputer.cpp                                        $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/LinearMath/btGeometryUtil.cpp                                              $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/LinearMath/btPolarDecomposition.cpp                                        $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/LinearMath/btQuickprof.cpp                                                 $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/LinearMath/btSerializer.cpp                                                $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	$(CXX) -c $(BULLET3D_SRC)/LinearMath/btVector3.cpp                                                   $(BULLET3D_CFLAGS) $(BULLET3D_INC)
	ar rcs  $(CORE_HOME)bullet3d.a $(CORE_HOME)*.o
	rm *.o
mini_mbm:
	$(CXX) -c main.cpp $(INCLUDES) $(LIB_PATH) $(FLAGS) $(DEF)
	$(CXX) main.o -o mini_mbm.exe $(INCLUDES) $(LIB_PATH) $(FLAGS) -g $(DEF) $(LIBS) lua-core.a lua.a core_mbm.a sqlite3.a box-2d.a bullet3d.a
	rm *.o
	rm *.a
clean:
	rm *.exe *.a *.o
