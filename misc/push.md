# Rack Push

JavaScript in browsers can request a WebSockets or SSE connection. The server
will detect this attempt by looking at the Upgrade header value or for SSE the
Content-Type. When detected the on_open method of app object registered by
rackup is called with a sender object. That object responds to a send with a
single argument that responds to to_s. When the connection is closed the
on_close method is called if the app responds to that methid.

In addition to the callbacks the server can implement a broadcast method that
takes two arguments, a path and a data object. The path is is used to match
against connections established and sends the data on any matching
connections.

```ruby
require "rack"
require 'agoo'

class FlyHandler

  attr_accessor: sender
  
  # Normal HTTP request handler.
  def call(req)
    # This part is only needed for Iodine.
    if env['HTTP_UPGRADE'.freeze] =~ /websocket/i
      env['upgrade.websocket'.freeze] = self
      return [0, {}, []]
    end
    [ 200, { }, [ "flying fish clock" ] ]
  end

  # This method is called when a WebSockets or SSE connection is
  # established. The sender argument can be used later to publish data to the
  # browser. The sender has a send method that takes a single argument that
  # responds to a to_s method.
  def on_open(sender)
    # Note that the sender can become invalid at any time if the connection is
    # closed. It will raise an exception on send if that occurs.
    @sender = sender
    Thread.new {
      app = self
      while !app.sender.nil? do
	now = Time.now
	s.send("%02d:%02d:%02d" % [now.hour, now.min, now.sec])
      end
    }
  end

  def on_close
    # Shutdown the time publisher.
    @sender = nil
  end

  # This method is optional as not all endpoints need to receive from the
  # browser.
  def on_message(data)
    puts "received #{data}"
  end

  # This is only needed for Iodine compatibility but can be used in any case.
  def write(data)
    @sender.send(data) unless @sender.nil?
  end

end

run FlyHandler.new

# A minimal startup of the Agoo rack handle using rackup and expecting a
# WebSockets or SSE connection. Note this does not allow for loading any
# static assets.
# $ bundle exec rackup

# Make requests on port 9292 to received responses.
```

