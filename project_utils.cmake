# heaphook implementations
set(HEAPHOOK_SOURCES
  ${heaphook_SOURCE_DIR}/src/heaphook/heaptracer.cpp
  ${heaphook_SOURCE_DIR}/src/heaphook/hook_functions.cpp
  ${heaphook_SOURCE_DIR}/src/heaphook/heaphook.cpp
  ${heaphook_SOURCE_DIR}/src/heaphook/utils.cpp)

# build_library function
function(build_library LIB_NAME_AND_SOURCES)
  list(GET ARGV 0 LIB_NAME)
  list(REMOVE_AT ARGV 0)
  set(SOURCES ${ARGV})

  add_library(${LIB_NAME} SHARED 
    ${SOURCES} 
    ${HEAPHOOK_SOURCES})

  target_include_directories(${LIB_NAME}
    PRIVATE ${heaphook_SOURCE_DIR}/include)
  
  set_target_properties(${LIB_NAME} PROPERTIES LINK_FLAGS "-Wl,--version-script=${heaphook_SOURCE_DIR}/Versions")

  install(TARGETS ${LIB_NAME} DESTINATION lib)
endfunction()

# test_library function
function(test_library LIB_NAME_AND_SOURCES) # === test_library ===
  list(GET ARGV 0 TEST_NAME)
  list(REMOVE_AT ARGV 0)
  set(SOURCES ${ARGV})

  ament_add_gtest(${TEST_NAME} 
    ${heaphook_SOURCE_DIR}/test/test_allocator.cpp 
    ${SOURCES} 
    ${HEAPHOOK_SOURCES})
  
  target_include_directories(${TEST_NAME}
    PRIVATE ${heaphook_SOURCE_DIR}/include)
  
  install(TARGETS ${TEST_NAME} DESTINATION lib)
endfunction() # === test_library ===
