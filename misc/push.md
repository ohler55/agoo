# Rack Push

Realtime websites are essential for some domains. A web site dispaying stock
prices or race progress would not be very useful if the displays were not kept
up to date. There is of course contant refreshes or polling but that puts a
heavy demand on the server and it is not real time.

Fortunately there is technology to push data to a web page. WebSocket and SSE
provide a means to push data to a web page. Coupled with Javascript pages can
be dynamic and display data as it changes.

Sadly, if using Ruby the has not been any real support for either WebSocket or
SSE. Using Rack as an example it was possible to hijack a connection and
manage all the interactions using raw IO calls but that is a task that is not
for the faint at heart. It is a non trivial exercise. Won't it be nice to have
a simple way of supporting push with Ruby.

There is a proposal to extend the Rack spec to include an option for WebSocket
and SSE. Currently two servers support that proposed
extension. [Agoo](https://https://github.com/ohler55/agoo) and
[Iodine](https://github.com/boazsegev/iodine) both high performance web server
gems that support push and implement the proposed Rack extension.

## Simple

Rack handlers are expected to have a `#call(env)` method that processes HTTP
requests. If the `env` argument to that call includes a non `nil` value for
`env['rack.upgrade?']` then the handle is expected to upgrade to a push
connection that is either WebSocket or SSE.

Acceptance of the connection upgrade is implemented by setting
`env['rack.upgrade'] to a new push handler. The push handler can implement
methods `#on_open`, `#on_close`, `#on_message`, `#on_drained`, and
`#on_shutdown`. These are all optional. The object is extended by the server
to have a `#write(msg)` and a `#pending` method. The `#write(msg)` method is
used to push data to web pages. The details are handled by the server. Just
call write and data appears at browser.

## Example

The example is a bit more than a hello world but only enough to make it
interesting. A browser is used to connect to a Rack server that runs a clock,
On each tick of the clock the time is sent to the browser. Either an SSE and a
WebSocket page can be used.

The example makes use of the Agoo server. Some variations in the
demultiplexing would be needed using the Iodine server but the core Ruby code
would be the same.

### websocket.html

The websocket.html page is a trivial example of how to use WebSockets.

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

The sse.html page is a trivial example of how to use SSE. SSE is the preferred
method for pushing events to mobile devices.

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

The push.rb file is an example of how to use push with the Agoo gem.

```ruby
require 'agoo'

# The websocket.html and sse.html are used for this example. After starting
# open a URL of http://localhost:6464/websocket.html or
# http://localhost:6464/sse.html.

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
