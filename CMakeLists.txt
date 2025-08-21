cmake_minimum_required(VERSION 3.13)
project(x5_ble_min C)

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Create the executable
add_executable(ble_min src/ble_min.c)

# gattlib 安装到了 /usr/local
find_library(GATTLIB gattlib PATHS /usr/local/lib NO_DEFAULT_PATH)
find_package(PkgConfig REQUIRED)
pkg_check_modules(BLUETOOTH REQUIRED bluez)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)

target_include_directories(ble_min PRIVATE
    ${GLIB2_INCLUDE_DIRS}
    /usr/local/include
)

target_link_libraries(ble_min
    ${GATTLIB}
    ${BLUETOOTH_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${GIO_LIBRARIES}
    ${GOBJECT_LIBRARIES}
)

# Debug configuration
target_compile_options(ble_min PRIVATE
    $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
)