# OpenSSL

set(ROOT_DIR_OPENSSL "${CMAKE_CURRENT_SOURCE_DIR}/openssl-3.5.2/build/android-arm64")

add_library(crypto STATIC IMPORTED GLOBAL)
set_target_properties(crypto PROPERTIES
        IMPORTED_LOCATION "${ROOT_DIR_OPENSSL}/lib/libcrypto.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ROOT_DIR_OPENSSL}/include"
)

add_library(ssl STATIC IMPORTED GLOBAL)
set_target_properties(ssl PROPERTIES
        IMPORTED_LOCATION "${ROOT_DIR_OPENSSL}/lib/libssl.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ROOT_DIR_OPENSSL}/include"
)

# cpp-httplib

add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib-0.26.0")

#target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib-0.26.0")
