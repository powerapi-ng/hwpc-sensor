cmake_minimum_required(VERSION 2.8)
project(smartwatts-sensor LANGUAGES C)

# Compiler/linker flags:
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Werror")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2")

# External libraries:
find_package(PkgConfig)
pkg_check_modules(CZMQ REQUIRED libczmq)
#pkg_check_modules(PFM REQUIRED libpfm)
pkg_check_modules(CGROUP REQUIRED libcgroup)
pkg_check_modules(BSON REQUIRED libbson-1.0)
pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)

add_executable(smartwatts-sensor
    src/config.h
    src/util.h src/util.c
    src/docker.h src/docker.c
    src/pmu.h src/pmu.c
    src/events.h src/events.c
    src/hwinfo.h src/hwinfo.c
    src/cgroups.h src/cgroups.c
    src/payload.h src/payload.c
    src/report.h src/report.c
    src/perf.h src/perf.c
    src/storage.h src/storage.c
    src/mongodb.c src/mongodb.h
    src/sensor.c
)

target_include_directories(smartwatts-sensor PRIVATE "${CZMQ_INCLUDE_DIRS}" "${CGROUP_INCLUDE_DIRS}" "${BSON_INCLUDE_DIRS}" "${MONGOC_INCLUDE_DIRS}")
target_compile_definitions(smartwatts-sensor PRIVATE "${CZMQ_DEFINITIONS}" "${CGROUP_DEFINITIONS}" "${BSON_DEFINITIONS}" "${MONGOC_DEFINITIONS}")
target_link_libraries(smartwatts-sensor "${CZMQ_LIBRARIES}" pfm "${HWLOC_LIBRARIES}" "${CGROUP_LIBRARIES}" "${BSON_LIBRARIES}" "${MONGOC_LIBRARIES}")

