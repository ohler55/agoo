
require 'agoo'

# Setting the thread count to 0 causes the server to use the current
# thread. Greater than zero runs the server in a separate thread and the
# current thread can be used for other tasks as long as it does not exit.
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

# Note the extra bind for https and the inclusion of a cert and key
# file. Those files can be generated in an ssl sub-directory with this
# command.
#
#  openssl req -x509 -newkey rsa:4096 -keyout ssl/key.pem -out ssl/cert.pem -nodes -days 365 -subj '/CN=localhost'
#
Agoo::Server.init(6464, 'root',
		  thread_count: 0,
		  ssl_cert: 'ssl/cert.pem',
		  ssl_key: 'ssl/key.pem',
		  bind: ['http://127.0.0.1:6464',
			 "https://127.0.0.1:6465",
			],
		 )

class MyHandler
  def call(req)
    [ 200, { }, [ "hello world" ] ]
  end
end

handler = MyHandler.new
# Register the handler before calling start.
Agoo::Server.handle(:GET, "/hello", handler)

Agoo::Server.start()

# To run this example type the following then go to a browser and enter a URL of https://localhost:6465/hello.
# ruby hello.rb
