#
# Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
include (GenerateExportHeader)

find_package(OpenSSL)

PREPEND(srcs_cryptographic "${CMAKE_CURRENT_LIST_DIR}/src"
    crypto_cipher.c
    crypto_key_exchange
    crypto_key_factory.c
    crypto_objects.c
    crypto_transform.c
    crypto_utils.c
    cryptography.c
)

add_library(dds_security_crypto SHARED "")

generate_export_header(
    dds_security_crypto
        BASE_NAME SECURITY
        EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/security/export.h"
)

add_definitions(-DDDSI_INCLUDE_SSL)

target_link_libraries(dds_security_crypto PUBLIC ddsc)
target_link_libraries(dds_security_crypto PUBLIC OpenSSL::SSL)

target_sources(dds_security_crypto
    PRIVATE
        ${srcs_cryptographic}
)

target_include_directories(dds_security_crypto
    PUBLIC
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_core,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)

install(
  TARGETS
  EXPORT "${PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
)
