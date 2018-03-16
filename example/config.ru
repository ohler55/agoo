# frozen_string_literal: true

require "rack"
require 'agoo'

class FlyHandler
  def call(req)
    [ 200, { }, [ "flying fish" ] ]
  end
end

run FlyHandler.new

# A minimal startup of the Agoo rack handle using rackup. Note this does not
# allow for loading any static assets.
# $ bundle exec rackup

# Make requests on port 9292 to received responses.
