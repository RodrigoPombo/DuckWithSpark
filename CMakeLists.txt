cmake_minimum_required(VERSION 3.5)

set(TARGET_NAME spark_duckdb)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})

if(WIN32)
  set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
find_package(OpenSSL REQUIRED)
find_package(ZLIB)

include_directories(src/include)
include_directories(duckdb/third_party/httplib)
include_directories(duckdb/third_party/yyjson/include)

set(EXTENSION_SOURCES
    src/spark_duckdb_extension.cpp
    src/auth.cpp
    src/http_client.cpp
    src/livy_client.cpp
    src/spark_execute.cpp
)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

target_compile_definitions(${EXTENSION_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(${LOADABLE_EXTENSION_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(${EXTENSION_NAME} OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(${LOADABLE_EXTENSION_NAME} OpenSSL::SSL OpenSSL::Crypto)

if(ZLIB_FOUND)
  target_compile_definitions(${EXTENSION_NAME} PRIVATE CPPHTTPLIB_ZLIB_SUPPORT)
  target_compile_definitions(${LOADABLE_EXTENSION_NAME} PRIVATE CPPHTTPLIB_ZLIB_SUPPORT)
  target_link_libraries(${EXTENSION_NAME} ZLIB::ZLIB)
  target_link_libraries(${LOADABLE_EXTENSION_NAME} ZLIB::ZLIB)
endif()

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
