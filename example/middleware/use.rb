
require 'agoo'

# Setting the thread count to 0 causes the server to use the current
# thread. Greater than zero runs the server in a separate thread and the
# current thread can be used for other tasks as long as it does not exit.
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

Agoo::Server.init(6464, 'root', thread_count: 0)

class MyHandler
  def self.call(env)
    [ 200, { }, [ "hello" ] ]
  end
end

class Middle
  def initialize(app, *args)
    @app = app
    @suffix = args[0]
  end

  def call(env)
    status, headers, body = @app.call(env)
    body[0] = "#{body[0]} #{@suffix}" if 0 < body.length
    [status, headers, body]
  end
end

class Outer
  def initialize(app, *args)
    @app = app
  end

  def call(env)
    status, headers, body = @app.call(env)
    body[0] = "#{body[0]} and universe" if 0 < body.length
    [status, headers, body]
  end
end

Agoo::Server.use(Outer)
Agoo::Server.use(Middle, "world")

# Register the handler before calling start.
Agoo::Server.handle(:GET, "/hello", MyHandler)

Agoo::Server.start()

# To run this example type the following then go to a browser and enter a URL of localhost:6464/hello.
# ruby hello.rb
