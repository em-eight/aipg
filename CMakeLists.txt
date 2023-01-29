cmake_minimum_required(VERSION 3.14)

project(aipg VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
  ppcdisasm
  GIT_REPOSITORY https://github.com/em-eight/ppcdisasm-cpp.git
  GIT_TAG        1ee2f06eb077431161c4ddf4239558de765767c5
)

FetchContent_MakeAvailable(ppcdisasm)

set (INJA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/inja-3.3.0/single_include)
set (JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/inja-3.3.0/third_party/include)
set (TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates)
set (AIPG_INCLUDE_FILE 
#  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/aipg/aipg.hpp>
#  $<INSTALL_INTERFACE:include/aipg/aipg.hpp>  # <prefix>/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/aipg/aipg.hpp  # generator expressions not working for compile definitions...
)

add_executable(aipg src/aipg.cpp)
target_include_directories(aipg
#  PUBLIC
#  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#  $<INSTALL_INTERFACE:include>  # <prefix>/include
  PRIVATE
  $<BUILD_INTERFACE:${JSON_INCLUDE_DIR}>
  $<BUILD_INTERFACE:${INJA_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>  # <prefix>/include
)
target_link_libraries(aipg PRIVATE ppcdisasm)
target_compile_definitions(aipg 
  PRIVATE -DTEMPLATES_DIR="${TEMPLATES_DIR}" -DCTX_INC_FILE="${AIPG_INCLUDE_FILE}"
)

file(GLOB IDIOM_FILES ${CMAKE_CURRENT_SOURCE_DIR}/test/idioms/*)
set(IDIOM_PARSER_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/src)
set(IDIOM_PARSER_INC_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${IDIOM_PARSER_SRC_DIR} ${IDIOM_PARSER_INC_DIR})

foreach (IDIOM_FILE ${IDIOM_FILES})
  get_filename_component(IDIOM_FILE_STEM ${IDIOM_FILE} NAME_WLE)
  set(IDIOM_PARSER_SRCS {IDIOM_PARSER_SRCS} ${IDIOM_PARSER_SRC_DIR}/${IDIOM_FILE_STEM}.cpp)
endforeach ()

add_custom_command(
  OUTPUT ${IDIOM_PARSER_SRCS}     # Treated as relative to CMAKE_CURRENT_BINARY_DIR
  COMMAND aipg --src_out ${IDIOM_PARSER_SRC_DIR} --inc_out ${IDIOM_PARSER_INC_DIR} ${IDIOM_FILES}
  DEPENDS aipg
)

add_executable(test test/test.cpp ${IDIOM_PARSER_SRCS})
target_include_directories(test
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>  # <prefix>/include
  ${IDIOM_PARSER_INC_DIR}
)
target_link_libraries(test ppcdisasm)
set_property(TARGET test PROPERTY CXX_STANDARD 20)

install(TARGETS aipg DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY ${INJA_INCLUDE_DIR}/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY ${JSON_INCLUDE_DIR}/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY ${TEMPLATES_DIR} DESTINATION share)