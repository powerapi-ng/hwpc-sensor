cmake_minimum_required(VERSION 2.8)
project(hwpc-sensor LANGUAGES C)
option(WITH_MONGODB "Build with support for MongoDB storage module" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Werror")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -D_FORTIFY_SOURCE=2")

find_package(PkgConfig)
pkg_check_modules(CZMQ REQUIRED libczmq)
#pkg_check_modules(PFM REQUIRED libpfm)
pkg_check_modules(CGROUP REQUIRED libcgroup)

set(SENSOR_SOURCES
    src/config.h
    src/util.h src/util.c
    src/target.h src/target.c
    src/pmu.h src/pmu.c
    src/events.h src/events.c
    src/hwinfo.h src/hwinfo.c
    src/cgroups.h src/cgroups.c
    src/payload.h src/payload.c
    src/report.h src/report.c
    src/perf.h src/perf.c
    src/storage.h src/storage.c
    src/csv.h src/csv.c
    src/sensor.c
)

set(SENSOR_INCLUDES "${CZMQ_INCLUDE_DIRS}" "${CGROUP_INCLUDE_DIRS}")
set(SENSOR_DEFINITIONS "${CZMQ_DEFINITIONS}" "${CGROUP_DEFINITIONS}")
set(SENSOR_LIBRARIES "${CZMQ_LIBRARIES}" "${CGROUP_LIBRARIES}" pfm)

if(WITH_MONGODB)
    pkg_check_modules(BSON REQUIRED libbson-1.0)
    pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)
    set(SENSOR_SOURCES "${SENSOR_SOURCES}" src/mongodb.c src/mongodb.h)
    set(SENSOR_INCLUDES "${SENSOR_INCLUDES}" "${BSON_INCLUDE_DIRS}" "${MONGOC_INCLUDE_DIRS}")
    set(SENSOR_DEFINITIONS "${SENSOR_DEFINITIONS}" "${BSON_DEFINITIONS}" "${MONGOC_DEFINITIONS}")
    set(SENSOR_LIBRARIES "${SENSOR_LIBRARIES}" "${BSON_LIBRARIES}" "${MONGOC_LIBRARIES}")
    add_definitions(-DHAVE_MONGODB)
endif()

add_executable(hwpc-sensor "${SENSOR_SOURCES}")
target_include_directories(hwpc-sensor PRIVATE "${SENSOR_INCLUDES}")
target_compile_definitions(hwpc-sensor PRIVATE "${SENSOR_DEFINITIONS}")
target_link_libraries(hwpc-sensor "${SENSOR_LIBRARIES}")

