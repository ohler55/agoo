#!/bin/sh

# Run each test separately since starting the server can only happen once in a
# process.

./log_test.rb
./static_test.rb
./base_handler_test.rb
./rack_handler_test.rb
