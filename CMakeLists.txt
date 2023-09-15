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

include(project_utils.cmake)

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

  # heaphook itself test
  ament_add_gtest(test_glibc test/test_heaphook.cpp)
  target_include_directories(test_glibc
    PRIVATE ${PROJECT_SOURCE_DIR}/include)

  ament_add_gtest(test_heaphook test/test_heaphook.cpp src/original_allocator ${HEAPHOOK_SOURCES})
  target_include_directories(test_heaphook
    PRIVATE ${PROJECT_SOURCE_DIR}/include)

  ament_add_gtest(test_utils test/test_utils.cpp)
  target_include_directories(test_utils
    PRIVATE ${PROJECT_SOURCE_DIR}/include)

  # allocator test
  test_library(test_original_allocator
    src/original_allocator.cpp)

  test_library(test_preloaded_tlsf
    src/tlsf/tlsf.cpp)
  target_link_libraries(test_preloaded_tlsf tlsf::tlsf)
endif()

add_executable(app src/app.cpp)

include(CheckSymbolExists)
check_symbol_exists(mallinfo2 malloc.h HAVE_MALLINFO2)

# build preloaded_heaptrack.so
build_library(preloaded_heaptrack src/original_allocator.cpp)
target_compile_options(preloaded_heaptrack PRIVATE "-DTRACE")
if(HAVE_MALLINFO2)
  target_compile_definitions(preloaded_heaptrack
    PRIVATE
      HAVE_MALLINFO2
  )
endif()

# build libpreloaded_tlsf.so
build_library(preloaded_tlsf src/tlsf/tlsf.cpp)
target_link_libraries(preloaded_tlsf PRIVATE tlsf::tlsf)
if(HAVE_MALLINFO2)
  target_compile_definitions(preloaded_tlsf
    PRIVATE
      HAVE_MALLINFO2
  )
endif()

build_library(original_allocator src/original_allocator.cpp)

# # This is a demonstration.
# build_library(my_allocator src/my_allocator.cpp)

install(TARGETS preloaded_heaptrack preloaded_tlsf DESTINATION lib)
install(TARGETS app DESTINATION bin)

ament_package()
