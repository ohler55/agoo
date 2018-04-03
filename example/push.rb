
require 'agoo'

# Setting the thread count to 0 causes the server to use the current
# thread. Greater than zero runs the server in a separate thread and the
# current thread can be used for other tasks as long as it does not exit.
#server = Agoo::Server.new(6464, '.', thread_count: 0)
server = Agoo::Server.new(6464, '.',
			  thread_count: 0,
			  log_states: {
			    INFO: true,
			    DEBUG: true,
			    connect: true,
			    request: true,
			    response: true,
			    eval: true,
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
    if (env['HTTP_Connection'].to_s.downcase.include?('upgrade') &&
	env['HTTP_Upgrade'].to_s.downcase == 'websocket') ||     # WebSockets
       env['HTTP_Accept'] == 'text/event-stream'  # SSE
      env['push.handler'] = TickTock.new(env)
      [ 101, { }, [ ] ]
    else
      [ 404, { }, [ ] ]
    end
  end
end

class TickTock
  def initialize(env)
    puts "*** ticktock initialize"
    @req = nil
    #subscribe("time")
  end

  def on_open()
    puts "*** on_open with #{@_req.class}"
  end

  def on_close
    #unsubscribe()
    # optional arg of subject previously subscribed to
    puts "*** closing"
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

# To run this example type the following then go to a browser and enter a URL of localhost:6464/hello.
# ruby hello.rb
