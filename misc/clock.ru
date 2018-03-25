# frozen_string_literal: true

require "rack"
require 'agoo'

$ticking = true
# Note that the sender can become invalid at any time if the connection is
# closed. It will raise an exception on send if that occurs.
$publish_thread = Thread.new {
  while $ticking do
    now = Time.now
    Agoo.publish channel: :time, message: ("%02d:%02d:%02d" % [now.hour, now.min, now.sec])
    sleep(1)
  end
}


class TickTock
  def initialize(env)
    @env = env
  end

  def on_open(sender = nil)
    # for iodine compatibility, as iodine doen't provide a sender object
    @sender = sender ? sender : self
    subscribe channel: :time
  end

  def on_close
    # Shutdown the time publisher.
    @ticking = false
  end

  # This method is optional as not all endpoints need to receive from the
  # browser.
  def on_message(data)
    puts "received #{data}"
    write("echo: #{data}")
  end

  # This is only needed for Iodine compatibility but can be used in any case.
  def write(data)
    # iodine code
    return super if defined?(super)
    # Agoo code
    @sender.write(data)
  end

end

class FlyHandler

  attr_accessor: sender
  
  # Normal HTTP request handler.
  def call(env)
    # This part is only needed for Iodine.
    if env['upgrade.websocket?'.freeze]
      env['upgrade.websocket'.freeze] = TickTock.new(env)
      return [0, {}, []]
    end
    [ 200, { }, [ "flying fish clock" ] ]
  end

end

run FlyHandler.new

# close the publishing thread gracefully
$ticking = false
$publish_thread.join

# A minimal startup of the Agoo rack handle using rackup and expecting a
# WebSockets or SSE connection. Note this does not allow for loading any
# static assets.
# $ bundle exec rackup

# Make requests on port 9292 to received responses.
