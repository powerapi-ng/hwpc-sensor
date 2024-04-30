cmake_minimum_required(VERSION 3.13)
project(hwpc-sensor LANGUAGES C)

option(WITH_MONGODB "Build with support for MongoDB storage module" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_compile_options(-Werror -Wall -Wextra -Wpedantic -Wformat=2 -Wnull-dereference -Wno-gnu-statement-expression)
add_link_options(-Wl,-z,relro,-z,now -pie)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2 -fstack-protector-strong")

set(SENSOR_SOURCES
    src/config.c
    src/util.c
    src/target.c
    src/target_docker.c
    src/target_kubernetes.c
    src/pmu.c
    src/events.c
    src/hwinfo.c
    src/payload.c
    src/report.c
    src/perf.c
    src/storage.c
    src/storage_null.c
    src/storage_csv.c
    src/storage_socket.c
    src/sensor.c
)

find_package(LibPFM REQUIRED)
find_package(PkgConfig)
pkg_check_modules(CZMQ REQUIRED libczmq)
pkg_check_modules(JSONC REQUIRED json-c)

if(WITH_MONGODB)
    pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)
    list(APPEND SENSOR_SOURCES src/storage_mongodb.c)
    add_compile_definitions(HAVE_MONGODB)
endif()

if(DEFINED ENV{GIT_TAG} AND DEFINED ENV{GIT_REV})
    add_compile_definitions(VERSION_GIT_TAG="$ENV{GIT_TAG}" VERSION_GIT_REV="$ENV{GIT_REV}")
endif()

add_executable(hwpc-sensor "${SENSOR_SOURCES}")
target_include_directories(hwpc-sensor SYSTEM PRIVATE "${LIBPFM_INCLUDE_DIRS}" "${CZMQ_INCLUDE_DIRS}" "${JSONC_INCLUDE_DIRS}" "${MONGOC_INCLUDE_DIRS}")
target_link_libraries(hwpc-sensor "${LIBPFM_LIBRARIES}" "${CZMQ_LIBRARIES}" "${JSONC_LIBRARIES}" "${MONGOC_LIBRARIES}")
