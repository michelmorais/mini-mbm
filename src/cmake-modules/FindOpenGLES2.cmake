#-------------------------------------------------------------------
# This file is stolen from part of the CMake build system for OGRE (Object-oriented Graphics Rendering Engine) http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

# - Try to find OpenGLES and EGL
# Once done this will define
#
#  OPENGLES2_FOUND        - system has OpenGLES
#  OPENGLES2_INCLUDE_DIR  - the GL include directory
#  OPENGLES2_LIBRARIES    - Link these to use OpenGLES
#
#  EGL_FOUND        - system has EGL
#  EGL_INCLUDE_DIR  - the EGL include directory
#  EGL_LIBRARIES    - Link these to use EGL

# Win32, Apple, and Android are not tested!
# Linux tested and works

if(WIN32)
	if(CYGWIN)
		find_path(OPENGLES2_INCLUDE_DIR GLES2/gl2.h)
		find_library(OPENGLES2_LIBRARY libGLESv2)
	else()
		if(BORLAND)
			set(OPENGLES2_LIBRARY import32 CACHE STRING "OpenGL ES 2.x library for Win32")
		else()
			# TODO
			# set(OPENGLES_LIBRARY ${SOURCE_DIR}/Dependencies/lib/release/libGLESv2.lib CACHE STRING "OpenGL ES 2.x library for win32"
		endif()
	endif()
elseif(APPLE)
	find_path(OPENGLES2_INCLUDE_DIR GLES2/gl2.h 
	    PATHS ${ANGLE}/include
		/opt/homebrew/include
	)
	find_path(EGL_INCLUDE_DIR EGL/egl.h
		PATHS ${ANGLE}/include
		/opt/homebrew/include
	)
	
	include_directories(${OPENGLES2_INCLUDE_DIR} ${EGL_INCLUDE_DIR})

	find_library(OPENGLES2_LIBRARY
		NAMES GLESv2
		PATHS /opt/homebrew/lib/
			${ANGLE}/lib
	)

	message(STATUS "Found OPENGLES2_LIBRARY at ${OPENGLES2_LIBRARY}")

	set(GOOGLE_CHROME_FRAMEWORK_LIBS
		"/Applications/Google Chrome.app/Contents/Frameworks/Google Chrome Framework.framework/Versions/Current/Libraries/libGLESv2.dylib"
	)
	# On MacOS, Google Chrome ships its own ANGLE build inside the app bundle.
	# We can try to link against those libraries if they exist.
	# Note that this is not a standard location, so this is more of a
	# convenience for developers who have Google Chrome installed.
	# If you have your own ANGLE build, you can set the ANGLE variable
	# to point to your build directory, and this script will pick up the
	# libraries from there instead.
	# Example:
	#   set(ANGLE /path/to/your/angle/build)
	#   cmake ..
	# This will make the script look for libraries in
	#
	#   $OGL_FOR_MAC/lib
	#
	
	find_library(OPENGLES1_gl_LIBRARY
		NAMES GLESv1_CM
		PATHS PATHS /opt/homebrew/lib/
			${ANGLE}/lib
			${GOOGLE_CHROME_FRAMEWORK_LIBS}
			${OGL_FOR_MAC}/lib
	)

	message(STATUS "Found OPENGLES1_gl_LIBRARY at ${OPENGLES1_gl_LIBRARY}")

	find_library(EGL_LIBRARY
			NAMES EGL
			PATHS PATHS /opt/homebrew/lib/
			${ANGLE}/lib
			${GOOGLE_CHROME_FRAMEWORK_LIBS}
			${OGL_FOR_MAC}/lib
	)

	message(STATUS "Found EGL_LIBRARY at ${EGL_LIBRARY}")

	# On Unix OpenGL usually requires X11.
		# It doesn't require X11 on OSX.

		if(OPENGLES2_LIBRARY)
			if(NOT X11_FOUND)
				message(STATUS "Looking for X11 for MacOS")
				include(FindX11)
			endif()
			if(X11_FOUND)
				message(STATUS "Included X11 for MacOS")
				set(OPENGLES2_LIBRARIES ${X11_LIBRARIES})
			endif()
		endif()

		find_library(VULKAN_LIBRARY
             NAMES vulkan.1
             HINTS /Users/michel/VulkanSDK/1.4.328.1/macOS/lib
			       /opt/homebrew/lib/
				   /usr/local/lib/
				   /usr/lib/
             REQUIRED)

		set(OPENGLES2_LIBRARIES ${OPENGLES2_LIBRARIES} ${VULKAN_LIBRARY})
		message(STATUS "Found Vulkan_LIBRARIES at ${VULKAN_LIBRARY}")

		message(STATUS "Set OPENGLES2_LIBRARIES to ${OPENGLES2_LIBRARIES}")

	set(EGL_LIBRARIES)
	set(OPENGLES2_FOUND TRUE)
else()
	find_path(OPENGLES2_INCLUDE_DIR GLES2/gl2.h
		PATHS /usr/openwin/share/include
			/opt/graphics/OpenGL/include
			/opt/vc/include
			/usr/X11R6/include
			/usr/include
	)

	find_library(OPENGLES2_LIBRARY
		NAMES GLESv2
		PATHS /opt/graphics/OpenGL/lib
			/usr/openwin/lib
			/usr/shlib /usr/X11R6/lib
			/opt/vc/lib
			/usr/lib/aarch64-linux-gnu
			/usr/lib/arm-linux-gnueabihf
			/usr/lib
	)

	find_library(OPENGLES1_gl_LIBRARY
		NAMES GLESv1_CM
		PATHS /opt/graphics/OpenGL/lib
			/usr/openwin/lib
			/usr/shlib /usr/X11R6/lib
			/opt/vc/lib
			/usr/lib/aarch64-linux-gnu
			/usr/lib/arm-linux-gnueabihf
			/usr/lib
	)
	if(NOT PLAT_LOWER STREQUAL "android")
	#if(NOT BUILD_ANDROID)
		find_path(EGL_INCLUDE_DIR EGL/egl.h
			PATHS /usr/openwin/share/include
				/opt/graphics/OpenGL/include
				/opt/vc/include
				/usr/X11R6/include
				/usr/include
		)

		find_library(EGL_LIBRARY
			NAMES EGL
			PATHS /opt/graphics/OpenGL/lib
				/usr/openwin/lib
				/usr/shlib
				/usr/X11R6/lib
				/opt/vc/lib
				/usr/lib/aarch64-linux-gnu
				/usr/lib/arm-linux-gnueabihf
				/usr/lib
		)

		# On Unix OpenGL usually requires X11.
		# It doesn't require X11 on OSX.

		if(OPENGLES2_LIBRARY)
			if(NOT X11_FOUND)
				include(FindX11)
			endif()
			if(X11_FOUND)
				set(OPENGLES2_LIBRARIES ${X11_LIBRARIES})
			endif()
		endif()
	endif()
endif()

set(OPENGLES2_LIBRARIES ${OPENGLES2_LIBRARIES} ${OPENGLES2_LIBRARY} ${OPENGLES1_gl_LIBRARY})

#if(BUILD_ANDROID)
if(NOT PLAT_LOWER STREQUAL "android")
	if(OPENGLES2_LIBRARY)
		set(EGL_LIBRARIES)
		set(OPENGLES2_FOUND TRUE)
	endif()
else()
	if(OPENGLES2_LIBRARY AND EGL_LIBRARY)
		set(OPENGLES2_LIBRARIES ${OPENGLES2_LIBRARY} ${OPENGLES2_LIBRARIES})
		set(EGL_LIBRARIES ${EGL_LIBRARY} ${EGL_LIBRARIES})
		set(OPENGLES2_FOUND TRUE)
	endif()
endif()

mark_as_advanced(
	OPENGLES2_INCLUDE_DIR
	OPENGLES2_LIBRARY
	OPENGLES1_gl_LIBRARY
	EGL_INCLUDE_DIR
	EGL_LIBRARY
)

if(OPENGLES2_FOUND)
	message(STATUS "Found system OpenGL ES 2 library: ${OPENGLES2_LIBRARIES}")
	message(STATUS "Found OPENGLES2_INCLUDE_DIR ${OPENGLES2_INCLUDE_DIR}")
else()
	set(OPENGLES2_LIBRARIES "")
	message(STATUS "Did NOT find system OpenGL ES 2 library")
endif()
