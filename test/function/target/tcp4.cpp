// SPDX-License-Identifier: Apache-2.0

/*
 * Copyright 2019-2020 Joel E. Anderson
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "test/helper/server.hpp"

#include <regex>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stumpless.h>
#include <gtest/gtest.h>
#include "test/function/rfc5424.hpp"
#include "test/helper/assert.hpp"
#include "test/helper/resolve.hpp"

#ifndef _WIN32
#  include <sys/socket.h>
#endif

#define BINDING_DISABLED_WARNING "some network tests will not run without the" \
                                 " ability to listen on a local socket to"     \
                                 " receive messages."

namespace {

  class Tcp4TargetTest : public::testing::Test {

    protected:
      struct stumpless_target *target;
      struct stumpless_entry *basic_entry;
      bool tcp_fixtures_enabled = true;
      char buffer[2048];
      const char *port = "514";
      socket_handle_t handle;
      socket_handle_t accepted = BAD_HANDLE;

    virtual void
    SetUp( void ) {
      struct stumpless_element *element;
      struct stumpless_param *param;

      // setting up to receive the sent messages
      handle = open_tcp_server_socket( AF_INET, "127.0.0.1", port );
      if( handle == BAD_HANDLE ) {
        printf( "WARNING: " BINDING_DISABLED_WARNING "\n" );
        tcp_fixtures_enabled = false;
      }

      target = stumpless_open_tcp4_target( "test-self",
                                           "127.0.0.1",
                                           STUMPLESS_OPTION_NONE,
                                           STUMPLESS_FACILITY_USER );

      stumpless_set_target_default_app_name( target, "network-target-test" );
      stumpless_set_target_default_msgid( target, "default-message" );

      basic_entry = stumpless_new_entry( STUMPLESS_FACILITY_USER,
                                         STUMPLESS_SEVERITY_INFO,
                                         "stumpless-unit-test",
                                         "basic-entry",
                                         "basic test message" );

      element = stumpless_new_element( "basic-element" );
      stumpless_add_element( basic_entry, element );

      param = stumpless_new_param( "basic-param-name", "basic-param-value" );
      stumpless_add_param( element, param );
    }

    virtual void
    TearDown( void ) {
      stumpless_destroy_entry( basic_entry );
      stumpless_close_network_target( target );
      close_server_socket( handle );

      if( accepted != BAD_HANDLE ) {
        close_server_socket( accepted );
      }
    }

    void
    GetNextMessage( void ) {
      if( accepted == BAD_HANDLE ) {
        accepted = accept_tcp_connection( handle );
      }

      recv_from_handle( accepted, buffer, 1024 );
    }
  };

  TEST_F( Tcp4TargetTest, AddEntry ) {
    int result;
    int octet_count;
    char *syslog_msg;
    std::cmatch matches;
    std::regex octet_count_regex( "^(\\d+) (.*)$" );

    if( !tcp_fixtures_enabled ) {
      SUCCEED(  ) << BINDING_DISABLED_WARNING;

    } else {
      ASSERT_NOT_NULL( target );
      ASSERT_TRUE( stumpless_target_is_open( target ) );
      ASSERT_NOT_NULL( basic_entry );

      result = stumpless_add_entry( target, basic_entry );
      EXPECT_GE( result, 0 );
      EXPECT_NO_ERROR;

      GetNextMessage(  );

      if( !std::regex_match( buffer, matches, octet_count_regex ) ) {
        FAIL(  ) << "octet count was not at the beginning of the message";
      } else {
        octet_count = std::stoi( matches[1] );
        EXPECT_EQ( octet_count + 1 + matches[1].length(  ), strlen( buffer ) );
      }

      syslog_msg = buffer + matches[1].length(  ) + 1;
      TestRFC5424Compliance( syslog_msg );
    }
  }

  TEST_F( Tcp4TargetTest, GetTransportPort ) {
    const char *port_result;

    if( !tcp_fixtures_enabled ) {
      SUCCEED(  ) << BINDING_DISABLED_WARNING;

    } else {
      port_result = stumpless_get_transport_port( target );

      EXPECT_TRUE( port_result != NULL );
      EXPECT_TRUE( port_result != port );
      EXPECT_STREQ( port_result, port );
    }
  }

  TEST_F( Tcp4TargetTest, GetUdpMaxMessageSize ) {
    size_t result;
    const struct stumpless_error *error;

    if( !tcp_fixtures_enabled ) {
      SUCCEED(  ) << BINDING_DISABLED_WARNING;

    } else {
      ASSERT_NOT_NULL( target );
      ASSERT_TRUE( stumpless_target_is_open( target ) );

      result = stumpless_get_udp_max_message_size( target );
      EXPECT_EQ( result, 0 );
      EXPECT_ERROR_ID_EQ( STUMPLESS_TARGET_INCOMPATIBLE );
    }
  }

  TEST_F( Tcp4TargetTest, SetUdpMaxMessageSize ) {
    struct stumpless_target *result;
    const struct stumpless_error *error;

    if( !tcp_fixtures_enabled ) {
      SUCCEED(  ) << BINDING_DISABLED_WARNING;

    } else {
      ASSERT_NOT_NULL( target );
      ASSERT_TRUE( stumpless_target_is_open( target ) );

      result = stumpless_set_udp_max_message_size( target, 1500 );
      EXPECT_NULL( result );
      EXPECT_ERROR_ID_EQ( STUMPLESS_TARGET_INCOMPATIBLE );
    }
  }

  /* non-fixture tests */

  TEST( NetworkTargetNewTest, Basic ) {
    struct stumpless_target *target;

    target = stumpless_new_tcp4_target( "my-tcp4-target" );
    EXPECT_TRUE( target != NULL );
    EXPECT_TRUE( stumpless_get_error(  ) == NULL );

    EXPECT_FALSE( stumpless_target_is_open( target ) );

    stumpless_close_network_target( target );
  }

  TEST( NetworkTargetOpenTest, BadAddress ) {
    struct stumpless_target *target;
    const struct stumpless_error *error;

    target = stumpless_open_tcp4_target( "bad-ipv4-address",
                                         "256.256.256.256",
                                         STUMPLESS_OPTION_NONE,
                                         STUMPLESS_FACILITY_USER );
    EXPECT_NULL( target );
    EXPECT_ERROR_ID_EQ( STUMPLESS_ADDRESS_FAILURE );
  }

  TEST( NetworkTargetOpenTest, NullDestination ) {
    struct stumpless_target *target;
    const struct stumpless_error *error;

    target = stumpless_open_tcp4_target( "no-destination-provided",
                                         NULL,
                                         STUMPLESS_OPTION_NONE,
                                         STUMPLESS_FACILITY_USER );
    EXPECT_NULL( target );
    EXPECT_ERROR_ID_EQ( STUMPLESS_ARGUMENT_EMPTY );
  }

  TEST( NetworkTargetOpenTest, NullName ) {
    struct stumpless_target *target;
    const struct stumpless_error *error;

    target = stumpless_open_tcp4_target( NULL,
                                         "127.0.0.1",
                                         STUMPLESS_OPTION_NONE,
                                         STUMPLESS_FACILITY_USER );
    EXPECT_NULL( target );
    EXPECT_ERROR_ID_EQ( STUMPLESS_ARGUMENT_EMPTY );
  }

  TEST( NetworkTargetSetDestination, OpenTarget ) {
    struct stumpless_target *target;
    struct stumpless_target *target_result;
    struct stumpless_entry *entry;
    const char *original_destination = "127.0.0.1";
    const char *new_destination = "localhost";
    const char *destination_result;
    char buffer[2048];
    int add_result;
    socket_handle_t accepted;
    socket_handle_t port_handle;


    if( !name_resolves( new_destination, AF_INET ) ) {
      printf( "WARNING: %s did not resolve, so this test will be skipped\n", new_destination );
      SUCCEED(  ) <<  "the hostname did not resolve, so this test will be skipped";

    } else {
      port_handle = open_tcp_server_socket( AF_INET, original_destination, "514" );

      if( port_handle == BAD_HANDLE ) {
        printf( "WARNING: " BINDING_DISABLED_WARNING "\n" );
        SUCCEED(  ) <<  BINDING_DISABLED_WARNING;

      } else {
        target = stumpless_open_tcp4_target( "target-to-self",
                                             original_destination,
                                             STUMPLESS_OPTION_NONE,
                                             STUMPLESS_FACILITY_USER );
        EXPECT_NO_ERROR;
        ASSERT_NOT_NULL( target );

        destination_result = stumpless_get_destination( target );
        EXPECT_TRUE( destination_result != NULL );
        EXPECT_STREQ( destination_result, original_destination );

        entry = stumpless_new_entry( STUMPLESS_FACILITY_USER,
                                     STUMPLESS_SEVERITY_INFO,
                                     "stumpless-unit-test",
                                     "basic-entry",
                                     "basic test message" );
        EXPECT_TRUE( entry != NULL );

        add_result = stumpless_add_entry( target, entry );
        EXPECT_GE( add_result, 0 );

        accepted = accept_tcp_connection( port_handle );
        recv_from_handle( accepted, buffer, 2048 );
        EXPECT_TRUE( buffer[0] != '\0' );
        close_server_socket( accepted );

        EXPECT_TRUE( stumpless_target_is_open( target ) );
        target_result = stumpless_set_destination( target, new_destination );
        EXPECT_NOT_NULL( target_result );
        EXPECT_NO_ERROR;

        EXPECT_TRUE( stumpless_target_is_open( target ) );

        destination_result = stumpless_get_destination( target );
        EXPECT_TRUE( destination_result != NULL );
        EXPECT_STREQ( destination_result, new_destination );

        add_result = stumpless_add_entry( target, entry );
        EXPECT_GE( add_result, 0 );

        accepted = accept_tcp_connection( port_handle );
        recv_from_handle( accepted, buffer, 2048 );
        EXPECT_TRUE( buffer[0] != '\0' );
        close_server_socket( accepted );

        stumpless_close_network_target( target );
        stumpless_destroy_entry( entry );
      }

      close_server_socket( port_handle );
    }
  }

  TEST( NetworkTargetSetDestination, PausedTarget ) {
    struct stumpless_target *target;
    struct stumpless_target *target_result;
    struct stumpless_entry *entry;
    const char *destination = "127.0.0.1";
    const char *destination_result;
    char buffer[2048];
    int add_result;
    socket_handle_t accepted;
    socket_handle_t port_handle;

    port_handle = open_tcp_server_socket( AF_INET, destination, "514" );

    if( port_handle == BAD_HANDLE ) {
      printf( "WARNING: " BINDING_DISABLED_WARNING "\n" );
      SUCCEED(  ) <<  BINDING_DISABLED_WARNING;

    } else {
      target = stumpless_new_tcp4_target( "target-to-self" );
      EXPECT_NO_ERROR;
      ASSERT_NOT_NULL( target );

      destination_result = stumpless_get_destination( target );
      EXPECT_NULL( destination_result );

      EXPECT_FALSE( stumpless_target_is_open( target ) );
      target_result = stumpless_set_destination( target, destination );
      EXPECT_NOT_NULL( target_result );
      EXPECT_FALSE( stumpless_target_is_open( target ) );

      destination_result = stumpless_get_destination( target );
      EXPECT_NOT_NULL( destination_result );
      EXPECT_STREQ( destination_result, destination );

      target_result = stumpless_open_target( target );
      ASSERT_NOT_NULL( target_result );
      EXPECT_EQ( target_result, target );
      EXPECT_NO_ERROR;

      EXPECT_TRUE( stumpless_target_is_open( target ) );

      entry = stumpless_new_entry( STUMPLESS_FACILITY_USER,
                                   STUMPLESS_SEVERITY_INFO,
                                   "stumpless-unit-test",
                                   "basic-entry",
                                   "basic test message" );
      EXPECT_NOT_NULL( entry );

      add_result = stumpless_add_entry( target, entry );
      EXPECT_GE( add_result, 0 );

      accepted = accept_tcp_connection( port_handle );
      recv_from_handle( accepted, buffer, 2048 );
      EXPECT_TRUE( buffer[0] != '\0' );
      close_server_socket( accepted );

      stumpless_close_network_target( target );
      stumpless_destroy_entry( entry );
    }

    close_server_socket( port_handle );
  }

  TEST( NetworkTargetSetTransportPort, OpenTarget ) {
    struct stumpless_target *target;
    struct stumpless_target *result;
    struct stumpless_entry *entry;
    const char *new_port = "515";
    const char *default_port;
    const char *current_port;
    char buffer[2048];
    int add_result;
    socket_handle_t accepted;
    socket_handle_t default_port_handle;
    socket_handle_t new_port_handle;

    default_port_handle = open_tcp_server_socket( AF_INET, "127.0.0.1", "514" );
    new_port_handle = open_tcp_server_socket( AF_INET, "127.0.0.1", new_port );

    if( default_port_handle != BAD_HANDLE && new_port_handle != BAD_HANDLE ) {
      target = stumpless_open_tcp4_target( "target-to-self",
                                           "127.0.0.1",
                                           STUMPLESS_OPTION_NONE,
                                           STUMPLESS_FACILITY_USER );
      ASSERT_NOT_NULL( target );

      entry = stumpless_new_entry( STUMPLESS_FACILITY_USER,
                                   STUMPLESS_SEVERITY_INFO,
                                   "stumpless-unit-test",
                                   "basic-entry",
                                   "basic test message" );
      ASSERT_NOT_NULL( entry );

      add_result = stumpless_add_entry( target, entry );
      EXPECT_GE( add_result, 0 );

      accepted = accept_tcp_connection( default_port_handle );
      recv_from_handle( accepted, buffer, 2048 );
      EXPECT_TRUE( buffer[0] != '\0' );
      close_server_socket( accepted );

      default_port = stumpless_get_transport_port( target );
      EXPECT_NOT_NULL( default_port );
      ASSERT_STRNE( default_port, new_port );

      result = stumpless_set_transport_port( target, new_port );
      EXPECT_NOT_NULL( result );

      current_port = stumpless_get_transport_port( target );
      EXPECT_NOT_NULL( current_port );
      EXPECT_NE( current_port, new_port );
      EXPECT_STREQ( new_port, current_port );
      EXPECT_NO_ERROR;

      add_result = stumpless_add_entry( target, entry );
      EXPECT_GE( add_result, 0 );

      accepted = accept_tcp_connection( new_port_handle );
      recv_from_handle( accepted, buffer, 2048 );
      EXPECT_TRUE( buffer[0] != '\0' );

      stumpless_destroy_entry( entry );
      close_server_socket( accepted );
      stumpless_close_network_target( target );

    }

    close_server_socket( default_port_handle );
    close_server_socket( new_port_handle );
  }

  TEST( NetworkTargetSetTransportPort, PausedTarget ) {
    struct stumpless_target *target;
    struct stumpless_target *result;
    struct stumpless_entry *entry;
    const char *destination = "127.0.0.1";
    const char *new_port = "515";
    const char *default_port;
    const char *current_port;
    char buffer[2048];
    int add_result;
    socket_handle_t accepted;
    socket_handle_t default_port_handle;
    socket_handle_t new_port_handle;

    default_port_handle = open_tcp_server_socket( AF_INET, destination, "514" );
    new_port_handle = open_tcp_server_socket( AF_INET, destination, new_port );

    if( default_port_handle != BAD_HANDLE && new_port_handle != BAD_HANDLE ) {
      target = stumpless_new_tcp4_target( "target-to-self" );
      ASSERT_NOT_NULL( target );

      default_port = stumpless_get_transport_port( target );
      EXPECT_NOT_NULL( default_port );
      ASSERT_STRNE( default_port, new_port );

      EXPECT_FALSE( stumpless_target_is_open( target ) );
      result = stumpless_set_transport_port( target, new_port );
      EXPECT_NOT_NULL( result );
      EXPECT_NO_ERROR;

      EXPECT_FALSE( stumpless_target_is_open( target ) );

      current_port = stumpless_get_transport_port( target );
      EXPECT_NOT_NULL( current_port );
      EXPECT_NE( current_port, new_port );
      EXPECT_STREQ( new_port, current_port );

      result = stumpless_set_destination( target, destination );
      EXPECT_NOT_NULL( result );

      result = stumpless_open_target( target );
      ASSERT_NOT_NULL( result );
      EXPECT_EQ( result, target );
      EXPECT_TRUE( stumpless_target_is_open( target ) );

      entry = stumpless_new_entry( STUMPLESS_FACILITY_USER,
                                   STUMPLESS_SEVERITY_INFO,
                                   "stumpless-unit-test",
                                   "basic-entry",
                                   "basic test message" );
      ASSERT_NOT_NULL( entry );

      add_result = stumpless_add_entry( target, entry );
      EXPECT_GT( add_result, 0 );
      EXPECT_NO_ERROR;

      accepted = accept_tcp_connection( new_port_handle );
      recv_from_handle( accepted, buffer, 2048 );
      EXPECT_TRUE( buffer[0] != '\0' );

      stumpless_destroy_entry( entry );
      close_server_socket( accepted );
      stumpless_close_network_target( target );

    }

    close_server_socket( default_port_handle );
    close_server_socket( new_port_handle );
  }
}
