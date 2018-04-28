# Rack Push

# TBD something interesting about rack, ruby push

realtime websites use websockets and sse (push)
  race results, operations, stocks, waiting for actions to finish, async query results, ...

won't it be nice to be able to support push with ruby rack

now only hijack
- no callback mechanism
- round peg in a square hole
 - assumes raw IO
 - convoluted multi-step to makes something even partially workable

proposed spec addition for eactly that and two servers already support that spec, Agoo and Iodine

proposed spec and implementation make it easy to push

## How To



## Implementation

TBD

### websocket.html

TBD

```html
<html>
  <body>
    <p id="status"> ... </p>
    <p id="message"> ... waiting ... </p>

    <script type="text/javascript">
      var sock;
      var url = "ws://" + document.URL.split('/')[2] + '/listen'
      if (typeof MozWebSocket != "undefined") {
          sock = new MozWebSocket(url);
      } else {
          sock = new WebSocket(url);
      }
      sock.onopen = function() {
          document.getElementById("status").textContent = "connected";
      }
      sock.onmessage = function(msg) {
          document.getElementById("message").textContent = msg.data;
      }
    </script>
  </body>
</html>
```

### sse.html

TBD

```html
<html>
  <body>
    <p id="status"> ... </p>
    <p id="message"> ... waiting ... </p>

    <script type="text/javascript">
    var src = new EventSource('sse');
    src.onopen = function() {
        document.getElementById('status').textContent = 'connected';
    }
    src.onerror = function() {
        document.getElementById('status').textContent = 'not connected';
    }
    function doSet(e) {
        document.getElementById("message").textContent = e.data;
    }
    src.addEventListener('msg', doSet, false);
    </script>
  </body>
</html>
```

### push.rb

TBD

```ruby
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
		      INFO: true,
		      DEBUG: true,
		      connect: true,
		      request: true,
		      response: true,
		      eval: true,
		      push: true,
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
      env['rack.upgrade'] = TickTock.new(env)
      [ 200, { }, [ ] ]
    else
      [ 404, { }, [ ] ]
    end
  end
end

$tt = nil

# A class to handle published times. It works with both WebSocket and SSE.
class TickTock
  def initialize(env)
    $tt = self
  end

  def on_open()
    puts "--- on_open with #{@_cid}"
  end

  def on_close
    puts "--- closing"
    $tt = nil
  end

  def on_drained()
    puts "--- on_drained"
  end

  def on_message(data)
    puts "--- received #{data}"
    write("echo: #{data}")
  end

end

# Register the handler before calling start. Agoo allows for demultiplexing at
# the server instead of in the call() method.
Agoo::Server.handle(:GET, "/hello", HelloHandler.new)
Agoo::Server.handle(:GET, "/listen", Listen.new)
Agoo::Server.handle(:GET, "/sse", Listen.new)

# With a thread_count greater than 0 the call to start returns after creating
# a server thread. If the count is 0 then the call only returned when the
# server is shutdown.
Agoo::Server.start()

# A simple clock publisher of sorts. It writes teh current time every second
# to the current listener. To support multiple listeners an array or some
# other collection could be used instead of a single value variable.
loop do
  now = Time.now

  unless $tt.nil?
    $tt.write("%02d:%02d:%02d" % [now.hour, now.min, now.sec])
  end
  sleep(1)
end

# This example does not use the config.ru approach as the Agoo demultiplexer
# is used to support several different connection types.

# To run this example:
# ruby push.rb
```
