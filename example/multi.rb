
require 'agoo'

Agoo::Log.configure(dir: '',
		    console: true,
		    classic: true,
		    colorize: true,
		    states: {
		      INFO: true,
		      DEBUG: false,
		      connect: true,
		      request: true,
		      response: true,
		      eval: true,
		      push: false,
		    })

Agoo::Server.init(6464, 'root', thread_count: 4)

class MyHandler
  def call(req)
    [ 200, { }, [ "hello world" ] ]
  end
end

handler = MyHandler.new
# Register the handler before calling start.
Agoo::Server.handle(:GET, "/hello", handler)

Agoo::Server.start()

# Agoo::Server.start() returns immediately after starting the handler
# threads. This allows additional work to take place after the start. If there
# is no other action needed other than to let the server handle requests then
# a simple sleep loop can be used to keep the app from exiting.
while true
  sleep(1)
end

# To run this example type the following then go to a browser and enter a URL of localhost:6464/hello.
# ruby hello.rb
