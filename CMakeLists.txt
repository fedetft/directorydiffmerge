
cmake_minimum_required(VERSION 3.1)
project(DIRECTORYDIFFMERGE)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
add_definitions(-UNDEBUG) # Enable asserts even in Release

## Target
set(DDM_SRCS main.cpp core.cpp extfs.cpp)
add_executable(ddm ${DDM_SRCS})

find_library(CRYPTOPP cryptopp)
target_link_libraries(ddm ${CRYPTOPP})
set(BOOST_LIBS program_options)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(ddm ${Boost_LIBRARIES})
