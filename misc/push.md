# A New Rack Push

Realtime websites are essential for some domains. A web site displaying stock
prices or race progress would not be very useful if the displays were not kept
up to date. It is of course possible to use constant refreshes or polling but
that puts a heavy demand on the server and it is not real time.

Fortunately there is technology to push data to a web page. WebSocket and SSE
provide a means to push data to a web page. Coupled with Javascript pages can
be dynamic and display data as it changes.

Up until now, Ruby support for WebSockets and SSE was very cumbersome,
resulting in poor performance and higher memory bloat despite well authored
gems such as Faye and ActionCable.

Using Rack as an example it was possible to hijack a connection and manage all
the interactions using raw IO calls but that is a task that is not for the
faint at heart. It is a non trivial exercise and it requires duplication of
the network handling logic. Basically it's almost like running two servers
instead of one.

Won't it be nice to have a simple way of supporting push with Ruby? Something
that works with the server rather than against it? Something that avoids the
need to run another "server" on the same process?

There is a proposal to extend the Rack spec to include an option for WebSocket
and SSE. Currently two servers support that proposed
extension. [Agoo](https://https://github.com/ohler55/agoo) and
[Iodine](https://github.com/boazsegev/iodine) both high performance web server
gems that support push and implement the proposed Rack extension. A copy of
the proposed spec in HTML format is
[here](http://www.ohler.com/agoo/rack/file.SPEC.html).

## Simple Callback Design

The Rack extension proposal uses a simple callback design. This design
replaces the need for socket hijacking, removes the hassle of socket
management and decouples the application from the network protocol layer.

All Rack handlers already have a `#call(env)` method that processes HTTP
requests. If the `env` argument to that call includes a non `nil` value for
`env['rack.upgrade?']` then the handle is expected to upgrade to a push
connection that is either WebSocket or SSE.

Acceptance of the connection upgrade is implemented by setting
`env['rack.upgrade']` to a new push handler. The push handler can implement
methods `#on_open`, `#on_close`, `#on_message`, `#on_drained`, and
`#on_shutdown`. These are all optional. A `client` object is passed as the
first argument to each method. This `client` can be used to check connection
status as well as writing messages to the connection.

The `client` has methods `#write(msg)`, `#pending`, `#open?`, and `#close`
method. The `#write(msg)` method is used to push data to web pages. The
details are handled by the server. Just call write and data appears at
browser.

## Examples

The example is a bit more than a hello world but only enough to make it
interesting. A browser is used to connect to a Rack server that runs a clock,
On each tick of the clock the time is sent to the browser. Either an SSE and a
WebSocket page can be used.

First some web pages will be needed. Lets call them `websocket.html` and
`sse.html`. Notice how similar they look.

```html
<!-- websocket.html -->
<html>
  <body>
    <p id="status"> ... </p>
    <p id="message"> ... waiting ... </p>

    <script type="text/javascript">
      var sock;
      var url = "ws://" + document.URL.split('/')[2] + '/upgrade'
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

```html
<!-- sse.html -->
<html>
  <body>
    <p id="status"> ... </p>
    <p id="message"> ... waiting ... </p>

    <script type="text/javascript">
    var src = new EventSource('upgrade');
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

With those two files there are a couple ways to implement the Ruby side.

A pure, `rackup` example works with any of the web server gems the support the
proposed additions to the Rack spec. Currently there are two gems that support
the additions. They are [Agoo](https://github.com/ohler55/agoo) and
[Iodine](https://github.com/boazsegev/iodine). Both are high performance
servers with some features unique to both.

One example is for Agoo only to demonstrate the use of the Agoo demultiplexing
but is otherewise identifcal to the pure Rack example.

The third, pub-sub example is also Agoo specific but with a few minor changes
it would be compatible with Iodine as well. The pub-sub example is a minimal
example of how publish and subscribe can be used with Rack.

### Pure Rack Example

Lets start with the server agnostic example that is just Rack with the
proposed extensions to the spec. Of course the file is named `config.ru`. The
file includes more than is necessary just work but some options methods are
added to make it clear when they are called. Since it is a 'hello world'
example there is the obligatory handled for returning that result. Comments
have been stripped out of the code in favor of explaining the code separately.

First the file then an explanation.

```ruby
# config.ru
require 'rack'
#require 'agoo'

class Clock
  def initialize()
    @clients = []
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

  def start
    loop do
      now = Time.now
      msg = "%02d:%02d:%02d" % [now.hour, now.min, now.sec]
      @mutex.synchronize {
        @clients.each { |c|
          puts "--- write failed" unless c.write(msg)
        }
      }
      sleep(1)
    end
  end
end

$clock = Clock.new

class Listen
  def call(env)
    path = env['SCRIPT_NAME'] + env['PATH_INFO']
    case path
    when '/'
      return [ 200, { }, [ "hello world" ] ]
    when '/websocket.html'
      return [ 200, { }, [ File.read('websocket.html') ] ]
    when '/sse.html'
      return [ 200, { }, [ File.read('sse.html') ] ]
    when '/upgrade'
      unless env['rack.upgrade?'].nil?
        env['rack.upgrade'] = $clock
        return [ 200, { }, [ ] ]
      end
    end
    [ 404, { }, [ ] ]
  end
end

Thread.new { $clock.start }
run Listen.new
```

#### Running

The example is run with this command.

```
$ bundle exec rackup -r agoo -s agoo
```

The server to use must be specified to use something other than the
default. Alternatively the commented out `require` for Agoo or an alternative
can be used by uncommenting the line.

```ruby
require 'agoo'
```

Once started open a browser and go to `http://localhost:9292/websocket.html`
or `http://localhost:9292/sse.html` and watch the page show 'connected' and
then showing the time as it changes every second.

#### Clock

The `Clock` class is the handler for upgraded calls. The `Listener` and
`Clock` could have been combined into a single class but for clarity they are
kept separate in this example.

The `Clock` object, `$clock` maintains a list of open connections. The default
configuration uses one thread but to be safe a mutex is used so that the same
example can be used with multiple threads configured. It's a good practice
when writing asynchrous callback code to assume there will be multiple threads
invoking the callbacks.

```ruby
  def initialize()
    @clients = []
    @mutex = Mutex.new
  end
```

The `#on_open` callback is called when a connection has been upgraded to a
WebSocket or SSE connection. That is the time to add the connection to the
client `client` to the `@clients` list is used to implement a broadcast write
or publish to all open connections. There are other options that will be
explored later.

```ruby
  def on_open(client)
    puts "--- on_open"
    @mutex.synchronize {
      @clients << client
    }
  end
```

When a connection is closed it should be removed the the `@clients` list. The
`#on_close` callback handles that. The other callbacks meerly print out a
statement that they have been invoked so that it is easier to trace what is
going on.

```ruby
  def on_close(client)
    puts "--- on_close"
    @mutex.synchronize {
      @clients.delete(client)
    }
  end
```

Finally, the clock is going to publish the time to all connections. The
`#start` method starts the clock and then every second the time is published
by writing to each connection.

```ruby
  def start
    loop do
      now = Time.now
      msg = "%02d:%02d:%02d" % [now.hour, now.min, now.sec]
      @mutex.synchronize {
        @clients.each { |c|
          puts "--- write failed" unless c.write(msg)
        }
      }
      sleep(1)
    end
  end
```

#### Listening

In a rackup started application, all request go to a single handler that has a
`#call(env)` method. Demultiplexing of the incoming request is done in the
`#call(env)` itself. Requests for static assets such as web pages have to be
handled.

```ruby
  def call(env)
    path = env['SCRIPT_NAME'] + env['PATH_INFO']
    case path
    when '/'
      return [ 200, { }, [ "hello world" ] ]
    when '/websocket.html'
      return [ 200, { }, [ File.read('websocket.html') ] ]
    when '/sse.html'
      return [ 200, { }, [ File.read('sse.html') ] ]
    when '/upgrade'
      unless env['rack.upgrade?'].nil?
        env['rack.upgrade'] = $clock
        return [ 200, { }, [ ] ]
      end
    end
    [ 404, { }, [ ] ]
  end
```

The proposed feature is the detection of a connection that wants to be
upgraded by checking the `env['rack.upgrade?']` variable. If that variables is
`:websocket` of `sse` then a handle should be provided by setting
`env['rack.upgrade']` to the upgrade handler. A return code of less than 300
will indicate to the server that the connection can be upgraded.

```ruby
    when '/upgrade'
      unless env['rack.upgrade?'].nil?
        env['rack.upgrade'] = $clock
        return [ 200, { }, [ ] ]
      end
    end
```

#### Starting

The last lines start the clock is a separate thread so that it runs in the
background and then rackup starts the server with a new listener.

```ruby
Thread.new { $clock.start }
run Listen.new
```

### Demultiplex

The `push.rb` is an example of using the Agoo demultiplexing feature. Allowing
the server to handle the demultiplexing simplifies `#call(env)` method and
allows each URL path to be handled by a separate object. This approach is
common among most web server in almost every language. But that is not the
only advantage. By setting a root directory for static resources Agoo can
serve up those resources without getting Ruby involved at all. This allows
those resources to be server many times faster all without creating additional
Ruby objects. Pages with significant statis resources become snappier and the
whole users experience is improved.

```ruby
# push.rb
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
		      push: true,
		    })
Agoo::Server.init(6464, '.', thread_count: 1)

class HelloHandler
  def call(env)
    [ 200, { }, [ "hello world" ] ]
  end
end

class Clock
  def initialize()
    @clients = []
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

  def start
    loop do
      now = Time.now
      msg = "%02d:%02d:%02d" % [now.hour, now.min, now.sec]
      @mutex.synchronize {
	@clients.each { |c|
          puts "--- write failed" unless c.write(msg)
	}
      }
      sleep(1)
    end
  end
end

$clock = Clock.new

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

Agoo::Server.handle(:GET, "/hello", HelloHandler.new)
Agoo::Server.handle(:GET, "/upgrade", Listen.new)

Agoo::Server.start()
$clock.start
```

The `push.rb` file is similar to the `config.ru` file with the same `Clock`
class. Running is different but still simple.

```
$ ruby push.rb
```

#### Logging

Agoo allows logging to be configured. The log is an asynchronous log so the
impact of logging on performance is minimal. The log is a feature based
logger. Most of the features are turned on so that it is clear what is
happening when running the example. Switch the `true` values to `false` to
turn off logging of any of the features listed.

```ruby
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
		      push: true,
		    })
```

#### Static Assets

The second argument to the Aggo server initializer sets the root directory for
static assets.

```ruby
Agoo::Server.init(6464, '.', thread_count: 1)
```

#### Listening

Listening becomes simplier just handling the upgrade.

```ruby
  def call(env)
    unless env['rack.upgrade?'].nil?
      env['rack.upgrade'] = $clock
      [ 200, { }, [ ] ]
    else
      [ 404, { }, [ ] ]
    end
  end
```

#### Demultiplexing

It is rare to have the behavior on a URL path to change after starting so why
not let the server handle the switching. The option to let the application
handle the demultiplexing in the `#call(env)` invocation but defining the
switching in one place is much easier to follow and manage especially a large
team of developers are working on a project.

```ruby
Agoo::Server.handle(:GET, "/hello", HelloHandler.new)
Agoo::Server.handle(:GET, "/upgrade", Listen.new)
```

### Simplified Pub-Sub

No reason to stop simplifying. With Agoo 2.1.0, publish and subscribe is
possible using string subjects. Instead of setting up the `@clients` array
attribute the Agoo server can take care of those details. This example is
slimmed down from the earlier example by making use of the Ruby feature that
classes are also object so the `Clock` class can act as the handler. Methods
not needed have been removed to leave just what is needed to publish time out
to all the listeners. Open a few pages with either or both the
`websocket.html` or `sse.html` files.

Note the use of `Agoo.publish` and `client.subscribe` in the `pubsub.rb`
example. Those two method are not part of the proposed Rack spec additions but
make stateless WebSocket and SSE use simple. Us this with the `websocket.html`
and `sse.html` as with the previous examples.

```ruby
# pubsub.rb
require 'agoo'

class Clock
  def self.call(env)
    unless env['rack.upgrade?'].nil?
      env['rack.upgrade'] = Clock
      [ 200, { }, [ ] ]
    else
      [ 404, { }, [ ] ]
    end
  end

  def self.on_open(client)
    client.subscribe('time')
  end

  Thread.new {
    loop do
      now = Time.now
      Agoo.publish('time', "%02d:%02d:%02d" % [now.hour, now.min, now.sec])
      sleep(1)
    end
  }
end

Agoo::Server.init(6464, '.', thread_count: 0)
Agoo::Server.handle(:GET, '/upgrade', Clock)
Agoo::Server.start()
```
