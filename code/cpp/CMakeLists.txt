project("SRMCollider")
cmake_minimum_required(VERSION 2.6)

set(CMAKE_BUILD_TYPE Release)

# All executables 
set(my_executables
test/testPrecursor
test/testgetNonUIS
test/testIntegrated
test/testRangetree
test/testeUIS
test/testLibrary
test/testCombinatorics
src/run_integrated
)

# All classes that will go into the library
set(my_sources
  src/calculate_eUIS.cpp
  src/combinatorics.h
  src/srmcolliderLib.cpp
  src/integratedrun.cpp
  src/rangetree.cpp
)

# CGAL is used header-only, no need for the libraries
find_package(CGAL REQUIRED)
message("Include dirs of CGAL: " ${CGAL_INCLUDE_DIRS} )
# message("Libs of CGAL: " ${CGAL_LIBRARIES} )

FIND_PACKAGE(PythonLibs 2.7 REQUIRED)
message("Include dirs of Python: " ${PYTHON_INCLUDE_DIRS} )
message("Libs of Python: " ${PYTHON_LIBRARIES} )

find_package(Boost COMPONENTS filesystem system unit_test_framework python REQUIRED)
message("Include dirs of boost: " ${Boost_INCLUDE_DIRS} )
message("Libs of boost: " ${Boost_LIBRARIES} )

include_directories(
  ${Boost_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
  ${CGAL_INCLUDE_DIRS}
  "./src/"
  "./"
)

# cgal requires the -frounding-math option
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frounding-math -Wall") 

add_library(SRMCollider SHARED ${my_sources})

## add targets for the executables
foreach(i ${my_executables})
  add_executable(${i} ${i}.C)
  target_link_libraries(${i} SRMCollider ${PYTHON_LIBRARIES} ${Boost_LIBRARIES} )
endforeach(i)

ENABLE_TESTING()
ADD_TEST(testgetNonUIS test/testgetNonUIS)
ADD_TEST(testPrecursor test/testPrecursor)
ADD_TEST(testIntegrated test/testIntegrated)
ADD_TEST(testRangetree test/testRangetree)
ADD_TEST(testeUIS test/testeUIS)
ADD_TEST(testLibrary test/testLibrary)
ADD_TEST(testCombinatorics test/testCombinatorics)
