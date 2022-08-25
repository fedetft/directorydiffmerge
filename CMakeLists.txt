
cmake_minimum_required(VERSION 3.1)
project(DIRECTORYDIFFMERGE)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
add_definitions(-UNDEBUG) # Enable asserts even in Release

# Uses nearly half the RAM at the cost of speed.
# In most use cases most of the time is however spent computing SHA1 hashses, so
# the speed reduction is only marginal. If you really don't care about using
# twice the RAM and want to go as fast as possible, comment out this line.
add_definitions(-DOPTIMIZE_MEMORY)

## Target
set(DDM_SRCS main.cpp backup.cpp core.cpp extfs.cpp)
add_executable(ddm ${DDM_SRCS})

find_package(Threads REQUIRED)
target_link_libraries(ddm ${CMAKE_THREAD_LIBS_INIT})
find_library(CRYPTOPP cryptopp)
target_link_libraries(ddm ${CRYPTOPP})
set(BOOST_LIBS program_options)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(ddm ${Boost_LIBRARIES})
