
cmake_minimum_required(VERSION 3.1)
project(DISKDIFF)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
add_definitions(-UNDEBUG) # Enable assets even in Release

## Target
set(DISKDIFF_SRCS main.cpp diskdiff.cpp extfs.cpp)
find_library(CRYPTOPP cryptopp)
add_executable(diskdiff ${DISKDIFF_SRCS})
target_link_libraries(diskdiff ${CRYPTOPP})
set(BOOST_LIBS program_options)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(diskdiff ${Boost_LIBRARIES})
