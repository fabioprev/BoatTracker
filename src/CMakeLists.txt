cmake_minimum_required(VERSION 2.8)

project(BoatTracker)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -ggdb")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/modules/")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3")

find_package(OpenCV REQUIRED)
find_package(PCAP REQUIRED)
find_package(TBB REQUIRED)
find_package(Threads)
find_package(LibXml2 REQUIRED)
find_package(PTracking REQUIRED)

set(EXECUTABLE_OUTPUT_PATH ../bin)

include_directories(src)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${PCAP_INCLUDE_DIR})
include_directories(${TBB_INCLUDE_DIRS})
include_directories(${LIBXML2_INCLUDE_DIR})
include_directories(${PTracking_INCLUDE_DIR})

file(GLOB_RECURSE BoatTracker_src "*.cpp")

add_executable(BoatTracker ${BoatTracker_src})
target_link_libraries(BoatTracker ${OpenCV_LIBS} ${PCAP_LIBRARIES} ${TBB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${LIBXML2_LIBRARIES} ${PTracking_LIBRARY} pthread boost_system boost_thread)
