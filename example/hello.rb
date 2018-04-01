
require 'agoo'

# Setting the thread count to 0 causes the server to use the current
# thread. Greater than zero runs the server in a separate thread and the
# current thread can be used for other tasks as long as it does not exit.
server = Agoo::Server.new(6464, 'root', thread_count: 0,
			log_console: true,
			log_classic: true,
			log_colorize: true,
			log_states: {
			  INFO: true,
			  DEBUG: false,
			  connect: false,
			  request: false,
			  response: false,
			  eval: true,
			})

class MyHandler
  def call(req)
    [ 200, { }, [ "hello world" ] ]
  end
end

handler = MyHandler.new
# Register the handler before calling start.
server.handle(:GET, "/hello", handler)

server.start()

# To run this example type the following then go to a browser and enter a URL of localhost:6464/hello.
# ruby hello.rb
