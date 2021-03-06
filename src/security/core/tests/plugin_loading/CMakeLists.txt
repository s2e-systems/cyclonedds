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
include(GenerateExportHeader)
include(CUnit)

set(security_plugin_loading_test_sources
  "security_config.c"
  "plugin_loading.c")

add_cunit_executable(cunit_security_plugin_loading ${security_plugin_loading_test_sources})
target_include_directories(
  cunit_security_plugin_loading PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
  "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
  "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../core/ddsi/include>")

target_link_libraries(cunit_security_plugin_loading PRIVATE ddsc security_api)
target_include_directories(cunit_security_plugin_loading PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

set(CUnit_plugin_mock_dir ".")

# Let the cunit application know the location and name of the library.
configure_file("config_env.h.in" "config_env.h")

function(add_mock libname casename)
  PREPEND(srcs_mock_authentication_all_ok "${CMAKE_CURRENT_LIST_DIR}/plugin_mock/${casename}"
    "mock_${libname}.c")
  add_library("dds_security_${casename}" SHARED "")
  
  generate_export_header(
    "dds_security_${casename}"
    BASE_NAME SECURITY
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/dds/security/${casename}_export.h")
  
  set_target_properties(
    "dds_security_${casename}"
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY                ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY                ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_CURRENT_BINARY_DIR})

  #find_package(OpenSSL )
  #target_link_libraries("dds_security_${casename}" PUBLIC OpenSSL::SSL)
  target_link_libraries("dds_security_${casename}" PUBLIC ddsc)
  
  target_sources("dds_security_${casename}"
    PRIVATE
      ${srcs_mock_authentication_all_ok})
  target_include_directories("dds_security_${casename}"
    PUBLIC
      "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>"
      "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_core,INTERFACE_INCLUDE_DIRECTORIES>>"
      "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
endfunction()

#PLUGIN MOCKS
add_mock(authentication authentication_all_ok)
add_mock(authentication authentication_all_ok_other)
add_mock(authentication authentication_missing_function)
add_mock(authentication authentication_finalize_error)
add_mock(access_control access_control_all_ok)
add_mock(access_control access_control_missing_function)
add_mock(cryptography cryptography_all_ok)
add_mock(cryptography cryptography_missing_function)
add_mock(authentication authentication_init_error)
