// Copyright 2016 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include "rcl/client.h"

#include "rcl/rcl.h"

#include "example_interfaces/srv/add_two_ints.h"
#include "rosidl_generator_c/string_functions.h"

#include "../memory_tools/memory_tools.hpp"
#include "../scope_exit.hpp"
#include "rcl/error_handling.h"

class TestClientFixture : public ::testing::Test
{
public:
  rcl_node_t * node_ptr;
  void SetUp()
  {
    stop_memory_checking();
    rcl_ret_t ret;
    ret = rcl_init(0, nullptr, rcl_get_default_allocator());
    ASSERT_EQ(RCL_RET_OK, ret) << rcl_get_error_string_safe();
    this->node_ptr = new rcl_node_t;
    *this->node_ptr = rcl_get_zero_initialized_node();
    const char * name = "test_client_node";
    rcl_node_options_t node_options = rcl_node_get_default_options();
    ret = rcl_node_init(this->node_ptr, name, "", &node_options);
    ASSERT_EQ(RCL_RET_OK, ret) << rcl_get_error_string_safe();
    set_on_unexpected_malloc_callback([]() {ASSERT_FALSE(true) << "UNEXPECTED MALLOC";});
    set_on_unexpected_realloc_callback([]() {ASSERT_FALSE(true) << "UNEXPECTED REALLOC";});
    set_on_unexpected_free_callback([]() {ASSERT_FALSE(true) << "UNEXPECTED FREE";});
    start_memory_checking();
  }

  void TearDown()
  {
    assert_no_malloc_end();
    assert_no_realloc_end();
    assert_no_free_end();
    stop_memory_checking();
    set_on_unexpected_malloc_callback(nullptr);
    set_on_unexpected_realloc_callback(nullptr);
    set_on_unexpected_free_callback(nullptr);
    rcl_ret_t ret = rcl_node_fini(this->node_ptr);
    delete this->node_ptr;
    EXPECT_EQ(RCL_RET_OK, ret) << rcl_get_error_string_safe();
    ret = rcl_shutdown();
    EXPECT_EQ(RCL_RET_OK, ret) << rcl_get_error_string_safe();
  }
};

/* Basic nominal test of a client.
 */
TEST_F(TestClientFixture, test_client_nominal) {
  stop_memory_checking();
  rcl_ret_t ret;
  rcl_client_t client = rcl_get_zero_initialized_client();

  // Initialize the client.
  const char * topic_name = "add_two_ints";
  const char * expected_topic_name = "/add_two_ints";
  rcl_client_options_t client_options = rcl_client_get_default_options();

  const rosidl_service_type_support_t * ts = ROSIDL_GET_SRV_TYPE_SUPPORT(
    example_interfaces, AddTwoInts);
  ret = rcl_client_init(&client, this->node_ptr, ts, topic_name, &client_options);

  // Check the return code of initialization and that the service name matches what's expected
  ASSERT_EQ(RCL_RET_OK, ret) << rcl_get_error_string_safe();
  EXPECT_EQ(strcmp(rcl_client_get_service_name(&client), expected_topic_name), 0);

  auto client_exit = make_scope_exit(
    [&client, this]() {
      stop_memory_checking();
      rcl_ret_t ret = rcl_client_fini(&client, this->node_ptr);
      EXPECT_EQ(RCL_RET_OK, ret) << rcl_get_error_string_safe();
    });

  // Initialize the client request.
  example_interfaces__srv__AddTwoInts_Request req;
  example_interfaces__srv__AddTwoInts_Request__init(&req);
  req.a = 1;
  req.b = 2;

  // Check that there were no errors while sending the request.
  int64_t sequence_number = 0;
  ret = rcl_send_request(&client, &req, &sequence_number);
  EXPECT_EQ(sequence_number, 1);
  EXPECT_EQ(RCL_RET_OK, ret) << rcl_get_error_string_safe();
}


/* Testing the client init and fini functions.
 */
TEST_F(TestClientFixture, test_client_init_fini) {
  stop_memory_checking();
  rcl_ret_t ret;
  // Setup valid inputs.
  rcl_client_t client;

  const rosidl_service_type_support_t * ts = ROSIDL_GET_SRV_TYPE_SUPPORT(
    example_interfaces, AddTwoInts);
  const char * topic_name = "chatter";
  rcl_client_options_t default_client_options = rcl_client_get_default_options();

  // Try passing null for client in init.
  ret = rcl_client_init(nullptr, this->node_ptr, ts, topic_name, &default_client_options);
  EXPECT_EQ(RCL_RET_INVALID_ARGUMENT, ret) << rcl_get_error_string_safe();
  rcl_reset_error();

  // Try passing null for a node pointer in init.
  client = rcl_get_zero_initialized_client();
  ret = rcl_client_init(&client, nullptr, ts, topic_name, &default_client_options);
  EXPECT_EQ(RCL_RET_INVALID_ARGUMENT, ret) << rcl_get_error_string_safe();
  rcl_reset_error();

  // Check if null publisher is valid
  EXPECT_FALSE(rcl_client_is_valid(nullptr, nullptr));
  rcl_reset_error();

  // Check if zero initialized client is valid
  client = rcl_get_zero_initialized_client();
  EXPECT_FALSE(rcl_client_is_valid(&client, nullptr));
  rcl_reset_error();

  // Check that a valid client is valid
  client = rcl_get_zero_initialized_client();
  ret = rcl_client_init(&client, this->node_ptr, ts, topic_name, &default_client_options);
  EXPECT_EQ(RCL_RET_OK, ret) << rcl_get_error_string_safe();
  EXPECT_TRUE(rcl_client_is_valid(&client, nullptr));
  rcl_reset_error();

  // Try passing an invalid (uninitialized) node in init.
  client = rcl_get_zero_initialized_client();
  rcl_node_t invalid_node = rcl_get_zero_initialized_node();
  ret = rcl_client_init(&client, &invalid_node, ts, topic_name, &default_client_options);
  EXPECT_EQ(RCL_RET_NODE_INVALID, ret) << rcl_get_error_string_safe();
  rcl_reset_error();

  // Try passing null for the type support in init.
  client = rcl_get_zero_initialized_client();
  ret = rcl_client_init(
    &client, this->node_ptr, nullptr, topic_name, &default_client_options);
  EXPECT_EQ(RCL_RET_INVALID_ARGUMENT, ret) << rcl_get_error_string_safe();
  rcl_reset_error();

  // Try passing null for the topic name in init.
  client = rcl_get_zero_initialized_client();
  ret = rcl_client_init(&client, this->node_ptr, ts, nullptr, &default_client_options);
  EXPECT_EQ(RCL_RET_INVALID_ARGUMENT, ret) << rcl_get_error_string_safe();
  rcl_reset_error();

  // Try passing null for the options in init.
  client = rcl_get_zero_initialized_client();
  ret = rcl_client_init(&client, this->node_ptr, ts, topic_name, nullptr);
  EXPECT_EQ(RCL_RET_INVALID_ARGUMENT, ret) << rcl_get_error_string_safe();
  rcl_reset_error();

  // Try passing options with an invalid allocate in allocator with init.
  client = rcl_get_zero_initialized_client();
  rcl_client_options_t client_options_with_invalid_allocator;
  client_options_with_invalid_allocator = rcl_client_get_default_options();
  client_options_with_invalid_allocator.allocator.allocate = nullptr;
  ret = rcl_client_init(
    &client, this->node_ptr, ts, topic_name, &client_options_with_invalid_allocator);
  EXPECT_EQ(RCL_RET_INVALID_ARGUMENT, ret) << rcl_get_error_string_safe();
  rcl_reset_error();

  // Try passing options with an invalid deallocate in allocator with init.
  client = rcl_get_zero_initialized_client();
  client_options_with_invalid_allocator = rcl_client_get_default_options();
  client_options_with_invalid_allocator.allocator.deallocate = nullptr;
  ret = rcl_client_init(
    &client, this->node_ptr, ts, topic_name, &client_options_with_invalid_allocator);
  EXPECT_EQ(RCL_RET_INVALID_ARGUMENT, ret) << rcl_get_error_string_safe();
  rcl_reset_error();

  // An allocator with an invalid realloc will probably work (so we will not test it).

  // Try passing options with a failing allocator with init.
  client = rcl_get_zero_initialized_client();
  rcl_client_options_t client_options_with_failing_allocator;
  client_options_with_failing_allocator = rcl_client_get_default_options();
  client_options_with_failing_allocator.allocator.allocate = failing_malloc;
  client_options_with_failing_allocator.allocator.deallocate = failing_free;
  client_options_with_failing_allocator.allocator.reallocate = failing_realloc;
  ret = rcl_client_init(
    &client, this->node_ptr, ts, topic_name, &client_options_with_failing_allocator);
  EXPECT_EQ(RCL_RET_BAD_ALLOC, ret) << rcl_get_error_string_safe();
  rcl_reset_error();
}
