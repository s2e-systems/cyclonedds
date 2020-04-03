#
# Copyright(c) 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

include(CUnit)
include(GenerateDummyExportHeader)

add_cunit_executable(cunit_ddsrt_ipchange "ip_change.c")
target_link_libraries(
  cunit_ddsrt_ipchange PRIVATE ddsrt)
if(WIN32)
  target_link_libraries(
    cunit_ddsrt_ipchange PRIVATE newdev Shlwapi Setupapi)
endif()

generate_dummy_export_header(
  cunit_ddsrt
  BASE_NAME dds
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/export.h")


target_include_directories(
  cunit_ddsrt PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
target_include_directories(
  cunit_ddsrt_ipchange PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
