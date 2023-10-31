cmake_minimum_required(VERSION 3.0)

PROJECT(icwmp C)

ADD_DEFINITIONS(-Wall -Werror -Wformat -g)
ADD_DEFINITIONS(-D_GNU_SOURCE)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${PROJECT_SOURCE_DIR}")

OPTION(WITH_WOLFSSL "build with lib wolfssl" OFF)
OPTION(WITH_OPENSSL "build with lib openssl" OFF)
OPTION(WITH_MBEDTLS "build with lib mbedtls" OFF)

IF(NOT WITH_WOLFSSL AND NOT WITH_OPENSSL AND NOT WITH_MBEDTLS)
	MESSAGE(FATAL_ERROR "You must enable one of the SSL libraries: {'WOLFSSL','OPENSSL','MBEDTLS'}")
ENDIF()

FILE(GLOB ICWMP_SOURCES ${PROJECT_SOURCE_DIR}/src/*.c)

IF(WITH_WOLFSSL)
	SET(SSL_LIBS wolfssl)
	SET(SSL_LIBS crypto)
	add_compile_definitions(LWOLFSSL)
ENDIF(WITH_WOLFSSL)

IF(WITH_OPENSSL)
	SET(SSL_LIBS ssl)
	SET(SSL_LIBS crypto)
	add_compile_definitions(LOPENSSL)
ENDIF(WITH_OPENSSL)

IF(WITH_MBEDTLS)
	SET(SSL_LIBS mbedtls)
	SET(SSL_LIBS mbedcrypto)
	add_compile_definitions(LMBEDTLS)
ENDIF(WITH_MBEDTLS)

#[[Compile and install icwmpd]]
ADD_EXECUTABLE(icwmpd ${ICWMP_SOURCES})
TARGET_LINK_LIBRARIES(icwmpd pthread z m json-c uci ubox ubus blobmsg_json curl mxml uuid ${SSL_LIBS} ${CRYPTO_LIBS})
INSTALL(FILES icwmpd PERMISSIONS OWNER_EXECUTE DESTINATION usr/sbin)
INSTALL(DIRECTORY DESTINATION etc/icwmpd)
INSTALL(DIRECTORY DESTINATION var/run/icwmpd)

#[[Compile and install libcwmpdm.so]]
ADD_LIBRARY(cwmpdm SHARED ${PROJECT_SOURCE_DIR}/src/cwmp_dm/datamodel.c)
INSTALL(DIRECTORY DESTINATION etc/bbfdm/plugins)
INSTALL(TARGETS cwmpdm LIBRARY DESTINATION etc/bbfdm/plugins)
