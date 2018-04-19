
require 'agoo'

# Setting the thread count to 0 causes the server to use the current
# thread. Greater than zero runs the server in a separate thread and the
# current thread can be used for other tasks as long as it does not exit.
#server = Agoo::Server.new(6464, '.', thread_count: 0)
server = Agoo::Server.new(6464, '.',
			  thread_count: 1,
			  log_states: {
			    INFO: true,
			    DEBUG: true,
			    connect: true,
			    request: true,
			    response: true,
			    eval: true,
			    push: true,
			  }
			 )

class HelloHandler
  def call(env)
    [ 200, { }, [ "hello world" ] ]
  end
end

# Used for both SSE and WebSockets connections. WebSocket requests are
# detected by an env['HTTP_Upgrade'] value of 'websocket' while an SSE
# connection is detected by an env['HTTP_Accept'] value of
# 'text/event-stream'. In both cases a Push handler should be created and
# returned by setting env['push.handler'] to the new object. (env attribute
# decision is still pending)
class Listen
  # Only used for WebSockets or SSE upgrades.
  def call(env)
    unless env['rack.upgrade?'].nil?
      env['rack.upgrade'] = TickTock.new(env)
      [ 101, { }, [ ] ]
    else
      [ 404, { }, [ ] ]
    end
  end
end

$tt = nil

class TickTock
  def initialize(env)
    puts "*** ticktock initialize"
    $tt = self
  end

  def on_open()
    puts "*** on_open with #{@_cid}"
    subscribe("time")
  end

  def on_close
    #unsubscribe()
    # optional arg of subject previously subscribed to
    puts "*** closing"
    $tt = nil
  end

  def on_message(data)
    puts "received #{data}"
    write("echo: #{data}")
  end

end

# Register the handler before calling start. Agoo allows for demultiplexing at
# the server instead of in the call().
server.handle(:GET, "/hello", HelloHandler.new)
server.handle(:GET, "/listen", Listen.new)
server.handle(:GET, "/sse", Listen.new)

server.start()

loop do
  now = Time.now

  #Rack::Rack.publish('time', "%02d:%02d:%02d" % [now.hour, now.min, now.sec])

  # TBD remove, just for testing
  unless $tt.nil?
    puts "*** pending: #{$tt.pending}"
    $tt.write("%02d:%02d:%02d" % [now.hour, now.min, now.sec])
    $tt.close
  end
  
  sleep(1)
end
