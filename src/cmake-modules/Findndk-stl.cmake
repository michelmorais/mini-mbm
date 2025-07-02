# https://github.com/jomof/ndk-stl/blob/master/ndk-stl-config.cmake
# Copy shared STL files to Android Studio output directory so they can be
# packaged in the APK.
# Usage:
#
#   find_package(ndk-stl REQUIRED)
#
# or
#
#   find_package(ndk-stl REQUIRED PATHS ".")


if(NOT ${ANDROID_STL} MATCHES "_shared")
  message(STATUS "STL is not needed because ANDROID_STL is NOT shared")
  return()
endif()

# Old ndk,
#function(configure_shared_stl lib_path so_base)
#  message("Configuring STL ${so_base} for ${ANDROID_ABI}")
#  configure_file(
#    "${ANDROID_NDK}/sources/cxx-stl/${lib_path}/libs/${ANDROID_ABI}/lib${so_base}.so" 
#    "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${so_base}.so" 
#    COPYONLY)
#endfunction()

# Now I see that the path is something like
# from android-ndk-r25c/meta/abis.json

# android-ndk-r25c/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/arm-linux-androideabi/libc++_shared.so
# android-ndk-r25c/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/x86_64-linux-android/libc++_shared.so
# android-ndk-r25c/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/i686-linux-android/libc++_shared.so
# android-ndk-r25c/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so

function(configure_shared_stl lib_path so_base)
  message("Configuring STL ${so_base} for ${ANDROID_ABI}")
  # Configuring STL c++_shared for arm64-v8a

  file(READ "${ANDROID_NDK}/meta/abis.json" JSON_FILE)
  string(JSON TRIPLE GET "${JSON_FILE}" "${ANDROID_ABI}" "triple")
  configure_file(
    "${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/${TRIPLE}/lib${so_base}.so" 
    "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${so_base}.so" 
    COPYONLY)
endfunction()

if("${ANDROID_STL}" STREQUAL "libstdc++")
  # The default minimal system C++ runtime library.
elseif("${ANDROID_STL}" STREQUAL "gabi++_shared")
  # The GAbi++ runtime (shared).
  message(FATAL_ERROR "gabi++_shared was not configured by ndk-stl package")
elseif("${ANDROID_STL}" STREQUAL "stlport_shared")
  # The STLport runtime (shared).
  configure_shared_stl("stlport" "stlport_shared")
elseif("${ANDROID_STL}" STREQUAL "gnustl_shared")
  # The GNU STL (shared).
  configure_shared_stl("gnu-libstdc++/4.9" "gnustl_shared")
elseif("${ANDROID_STL}" STREQUAL "c++_shared")
  # The LLVM libc++ runtime (static).
  configure_shared_stl("llvm-libc++" "c++_shared")
else()
   message(FATAL_ERROR "STL configuration ANDROID_STL=${ANDROID_STL} is not supported")
endif()

