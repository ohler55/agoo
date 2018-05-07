
require 'agoo'

# The websocket.html and sse.html are used for this example. After starting
# open a URL of http://localhost:6464/websocket.html or
# http://localhost:6464/sse.html.

# The log is configured separately from the server. The log is ready without
# the configuration step but the default values will be used. All the states
# are set to true for the example but can be set to false to make the example
# less verbose.
Agoo::Log.configure(dir: '',
		    console: true,
		    classic: true,
		    colorize: true,
		    states: {
		      INFO: false,
		      DEBUG: false,
		      connect: false,
		      request: false,
		      response: false,
		      eval: false,
		      push: false,
		    })

# Setting the thread count to 0 causes the server to use the current
# thread. Greater than zero runs the server in a separate thread and the
# current thread can be used for other tasks as long as it does not exit.
#Agoo::Server.init(6464, '.', thread_count: 0)
Agoo::Server.init(6464, '.', thread_count: 1)

class HelloHandler
  def call(env)
    [ 200, { }, [ "hello world" ] ]
  end
end

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
	  begin
	    c.write(msg)
	  rescue Exception => e
	    # If the connection is closed while writing this could occur. Just
	    # the nature of async systems.
	    puts "--- write failed. #{e.class}: #{e.message}"
	  end
	}
      }
      sleep(1)
    end
  end
end

# Reuse the tick_tock instance.
$clock = Clock.new

# Used for both SSE and WebSocket connections. WebSocket requests are
# detected by an env['rack.upgrade?'] value of :websocket while an SSE
# connection is detected by an env['rack.upgrade?'] value of :sse.  In both
# cases a Push handler should be created and returned by setting
# env['rack.upgrade'] to the new object or handler. The handler will be
# extended to have a 'write' method as well as a 'pending' and a 'close'
# method.
class Listen
  # Only used for WebSocket or SSE upgrades.
  def call(env)
    unless env['rack.upgrade?'].nil?
      env['rack.upgrade'] = $clock
      [ 200, { }, [ ] ]
    else
      [ 404, { }, [ ] ]
    end
  end
end

# Register the handler before calling start. Agoo allows for de-multiplexing at
# the server instead of in the call() method.
Agoo::Server.handle(:GET, "/hello", HelloHandler.new)
Agoo::Server.handle(:GET, "/listen", Listen.new)
Agoo::Server.handle(:GET, "/sse", Listen.new)

# With a thread_count greater than 0 the call to start returns after creating
# a server thread. If the count is 0 then the call only returned when the
# server is shutdown.
Agoo::Server.start()

# Start the clock.
$clock.start

# This example does not use the config.ru approach as the Agoo de-multiplexer
# is used to support several different connection types instead of
# de-multiplexing in the #call method.

# To run this example:
# ruby push.rb

# After starting open a URL of http://localhost:6464/websocket.html or
# http://localhost:6464/sse.html.
