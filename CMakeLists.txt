cmake_minimum_required(VERSION 3.13)
project(hwpc-sensor LANGUAGES CXX)

option(WITH_MONGODB "Build with support for MongoDB storage module" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_compile_options(-Werror -Wall -Wextra -Wformat=2 -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE)
add_link_options(-pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack -Wl,-z,defs -Wl,--as-needed)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # These warnings need to be suppressed temporarily, as the only fix is to rewrite the code in C++
    add_compile_options(-Wno-deprecated -Wno-c99-designator -Wno-vla-cxx-extension)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Og -fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(SENSOR_SOURCES
    src/config_cli.c
    src/config_json.c
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

foreach(src ${SENSOR_SOURCES})
    set_source_files_properties(${src} PROPERTIES LANGUAGE CXX)
endforeach()

target_compile_features(hwpc-sensor PUBLIC cxx_std_20)
set_target_properties(hwpc-sensor PROPERTIES CXX_EXTENSIONS OFF LINKER_LANGUAGE CXX)

target_include_directories(hwpc-sensor SYSTEM PRIVATE "${LIBPFM_INCLUDE_DIRS}" "${CZMQ_INCLUDE_DIRS}" "${JSONC_INCLUDE_DIRS}" "${MONGOC_INCLUDE_DIRS}")
target_link_libraries(hwpc-sensor "${LIBPFM_LIBRARIES}" "${CZMQ_LIBRARIES}" "${JSONC_LIBRARIES}" "${MONGOC_LIBRARIES}")
