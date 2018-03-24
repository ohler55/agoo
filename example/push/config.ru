
require 'rack'
require 'agoo'

# The websocket.html and sse.html are used for this example. After starting
# open a URL of http://localhost:9292/websocket.html or
# http://localhost:9292/sse.html.

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
	env['rack.upgrade'] = TickTock.new(env)
	return [ 200, { }, [ ] ]
      end
    end
    [ 404, { }, [ ] ]
  end
end

# A place to keep track of active listeners.
$tt_list = []

# A mutex is needed if the server uses multiple threads for callbacks so to be
# on the safe side one is used here.
$tt_mutex = Mutex.new

# A class to handle published times. It works with both WebSocket and SSE.
class TickTock
  def initialize(env)
    $tt_mutex.synchronize {
      $tt_list << self
    }
  end

  def on_open()
    puts "--- on_open"
  end

  def on_close
    puts "--- closing"
    $tt_mutex.synchronize {
      $tt_list.delete(self)
    }
  end

  def on_drained
    puts "--- on_drained"
  end

  def on_message(data)
    puts "--- received #{data}"
    write("echo: #{data}")
  end

end

# A simple clock publisher of sorts. It writes the current time every second
# to the current listeners. It runs in a separate thread so that rackup can
# continue to process.
Thread.new do
  loop do
    now = Time.now
    $tt_mutex.synchronize {
      $tt_list.each { |tt|
	begin
	  tt.write("%02d:%02d:%02d" % [now.hour, now.min, now.sec])
	rescue IOError => e
	  # An IOError can occur if the connection has been closed while shutting down.
	  puts "#{e.class}: #{e.message}" unless e.message.include?('shutdown')
	end
      }
    }
    sleep(1)
  end
end

run Listen.new

# A minimal startup of the Agoo rack handle using rackup. Note this does not
# allow for loading any static assets.
# $ bundle exec rackup

# Make requests on port 9292 to received responses.
