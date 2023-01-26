cmake_minimum_required(VERSION 3.12)

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
message(${TEMPLATES_DIR})
message(${AIPG_INCLUDE_FILE})
target_link_libraries(aipg PRIVATE ppcdisasm)
target_compile_definitions(aipg 
  PRIVATE -DTEMPLATES_DIR="${TEMPLATES_DIR}" -DCTX_INC_FILE="${AIPG_INCLUDE_FILE}"
)

install(TARGETS aipg DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY ${INJA_INCLUDE_DIR}/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY ${JSON_INCLUDE_DIR}/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY ${TEMPLATES_DIR} DESTINATION share)