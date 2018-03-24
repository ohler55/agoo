# frozen_string_literal: true

require "rack"
require 'agoo'

class TickTock
  def initialize(env)
    @ticking = true
    # Note that the sender can become invalid at any time if the connection is
    # closed. It will raise an exception on send if that occurs.
    Thread.new {
      while @ticking do
	now = Time.now
	@sender.write("%02d:%02d:%02d" % [now.hour, now.min, now.sec])
      end
    }
  end
  
  def on_close
    # Shutdown the time publisher.
    @ticking = false
  end

  # This method is optional as not all endpoints need to receive from the
  # browser.
  def on_message(data)
    puts "received #{data}"
  end

  # This is only needed for Iodine compatibility but can be used in any case.
  def write(data)
    @sender.write(data)
  end

end

class FlyHandler

  attr_accessor: sender
  
  # Normal HTTP request handler.
  def call(env)
    # This part is only needed for Iodine.
    if env['HTTP_UPGRADE'.freeze] =~ /websocket/i
      env['upgrade.websocket'.freeze] = TickTock.new(env)
      return [0, {}, []]
    end
    [ 200, { }, [ "flying fish clock" ] ]
  end

  # This method is called when a WebSockets or SSE connection is
  # established. The env argument is the same as what would be given to the
  # call method except the env[`push.writer'] is set to a writer that can be
  # used to push or publish data to the browser.
  #
  # Return a handler for on_message and on_close callbacks. This is
  # optional. If nil is returned then broadcasts to the browsers are possible
  # but not direct write from Ruby are. If the call raises an exception then
  # the connection is closed.
  def on_open(env)
    TickTock.new(env)
  end

end

run FlyHandler.new

# A minimal startup of the Agoo rack handle using rackup and expecting a
# WebSockets or SSE connection. Note this does not allow for loading any
# static assets.
# $ bundle exec rackup

# Make requests on port 9292 to received responses.
