cmake_minimum_required(VERSION 3.8)
project(heaphook)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.

find_package(tlsf REQUIRED)

find_package(ament_cmake_gtest REQUIRED)

# heaphook implementations
set(HEAPHOOK_SOURCES
  src/heaphook/heaptracer.cpp
  src/heaphook/hook_functions.cpp
  src/heaphook/heaphook.cpp
  src/heaphook/utils.cpp)

# build library
function(build_library LIB_NAME_AND_SOURCES) # === build_library ===
  list(GET ARGV 0 LIB_NAME)
  list(REMOVE_AT ARGV 0)
  set(SOURCES ${ARGV})

  add_library(${LIB_NAME} SHARED ${SOURCES} ${HEAPHOOK_SOURCES})
  target_include_directories(${LIB_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/include)
  # version script
  set_target_properties(${LIB_NAME} PROPERTIES LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/Versions")

  install(TARGETS ${LIB_NAME}
    DESTINATION lib)
endfunction() # === build_library ===

function(test_library LIB_NAME_AND_SOURCES) # === test_library ===
  list(GET ARGV 0 TEST_NAME)
  list(REMOVE_AT ARGV 0)
  set(SOURCES ${ARGV})

  ament_add_gtest(${TEST_NAME} test/test_allocator.cpp ${SOURCES} ${HEAPHOOK_SOURCES})
  target_include_directories(${TEST_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/include)
  
  install(TARGETS ${TEST_NAME}
    DESTINATION lib)
endfunction() # === test_library ===

build_library(original_allocator
  src/original_allocator.cpp)
target_compile_options(original_allocator PRIVATE "-DTRACE")

build_library(tlsf_hook
  src/tlsf/tlsf.cpp)
target_link_libraries(tlsf_hook PRIVATE tlsf::tlsf)

build_library(my_allocator
  src/my_allocator.cpp)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  # ========== google test ==========
  # heaphook itself test
  ament_add_gtest(test_heaphook test/test_heaphook.cpp src/original_allocator ${HEAPHOOK_SOURCES})
  target_include_directories(test_heaphook
    PRIVATE ${PROJECT_SOURCE_DIR}/include)

  ament_add_gtest(test_utils test/test_utils.cpp)
  target_include_directories(test_utils
    PRIVATE ${PROJECT_SOURCE_DIR}/include)
  
  # allocator test
  test_library(test_original_allocator
    src/original_allocator.cpp)

  test_library(test_tlsf_hook
    src/tlsf/tlsf.cpp)
  target_link_libraries(test_tlsf_hook tlsf::tlsf)
  # =================================
endif()

add_executable(app src/app.cpp)

include(CheckSymbolExists)
check_symbol_exists(mallinfo2 malloc.h HAVE_MALLINFO2)

add_library(preloaded_heaptrack SHARED src/preloaded.cpp)
if(HAVE_MALLINFO2)
  target_compile_definitions(preloaded_heaptrack
    PRIVATE
      HAVE_MALLINFO2
  )
endif()

add_library(preloaded_tlsf SHARED src/preloaded_tlsf.cpp)
target_link_libraries(preloaded_tlsf PRIVATE tlsf::tlsf)
if(HAVE_MALLINFO2)
  target_compile_definitions(preloaded_tlsf
    PRIVATE
      HAVE_MALLINFO2
  )
endif()

install(TARGETS preloaded_heaptrack preloaded_tlsf DESTINATION lib)
install(TARGETS app DESTINATION bin)

ament_package()
