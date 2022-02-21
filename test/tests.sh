#!/bin/sh

# Run each test separately since starting the server can only happen once in a
# process.

echo "----- log_test.rb --------------------------------------------------------------"
./log_test.rb

echo "----- static_test.rb -----------------------------------------------------------"
./static_test.rb

echo "----- base_handler_test.rb -----------------------------------------------------"
./base_handler_test.rb

echo "----- rack_handler_test.rb -----------------------------------------------------"
./rack_handler_test.rb

echo "----- hijack_test.rb -----------------------------------------------------------"
./hijack_test.rb

echo "----- bind_test.rb -------------------------------------------------------------"
./bind_test.rb

echo "----- graphql_test.rb ----------------------------------------------------------"
./graphql_test.rb

echo "----- graphql_error_test.rb ----------------------------------------------------------"
./graphql_error_test.rb

echo "----- early_hints_test.rb ----------------------------------------------------------"
./early_hints_test.rb
