
require 'rack'
require 'agoo'

# The websocket.html and sse.html are used for this example. After starting
# open a URL of http://localhost:9292/websocket.html or
# http://localhost:9292/sse.html.

# A class to handle published times. It works with both WebSocket and
# SSE. Only a single instance is needed as the object keeps track of which
# connections or clients are open.
class Clock
  def initialize()
    @clients = []
    # A mutex is needed if the server uses multiple threads for callbacks so
    # to be on the safe side one is used here.
    @mutex = Mutex.new
  end

  def on_open(client)
    puts "--- on_open"
    @mutex.synchronize {
      @clients << client
    }
  end

  def on_close(client)
    puts "--- on_close"
    @mutex.synchronize {
      @clients.delete(client)
    }
  end

  def on_drained(client)
    puts "--- on_drained"
  end

  def on_message(client, data)
    puts "--- on_message #{data}"
    client.write("echo: #{data}")
  end

  # A simple clock publisher of sorts. It writes the current time every second
  # to the current clients.
  def start
    loop do
      now = Time.now
      msg = "%02d:%02d:%02d" % [now.hour, now.min, now.sec]
      @mutex.synchronize {
	@clients.each { |c|
          unless c.write(msg)
	    # If the connection is closed while writing this could occur. Just
	    # the nature of async systems.
	    puts "--- write failed"
	  end
	}
      }
      sleep(1)
    end
  end
end

# Reuse the tick_tock instance.
$clock = Clock.new

# The Listen class is used for both SSE and WebSocket connections. WebSocket
# requests are detected by an env['rack.upgrade?'] value of :websocket while
# an SSE connection is detected by an env['rack.upgrade?'] value of :sse.  In
# both cases a Push handler should be created and returned by setting
# env['rack.upgrade'] to the new object or handler. The handler will be
# extended to have a 'write' method as well as a 'pending' and a 'close'
# method.
class Listen
  # Used for WebSocket or SSE upgrades as well as for serving the
  # websocket.html and sse.html.
  def call(env)
    # de-multiplex the call.
    path = env['SCRIPT_NAME'] + env['PATH_INFO']
    case path
    when '/websocket.html'
      return [ 200, { }, [ File.read('websocket.html') ] ]
    when '/sse.html'
      return [ 200, { }, [ File.read('sse.html') ] ]
    when '/listen', '/sse'
      unless env['rack.upgrade?'].nil?
	env['rack.upgrade'] = $clock
	return [ 200, { }, [ ] ]
      end
    end
    [ 404, { }, [ ] ]
  end
end

# Start the clock.
Thread.new { $clock.start }

run Listen.new

# A minimal startup of the Agoo rack handle using rackup. Note this does not
# allow for loading any static assets.
# $ bundle exec rackup

# Make requests on port 9292 to received responses.
