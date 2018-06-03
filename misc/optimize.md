# Optimizing Agoo

Right out of the box Agoo is fast but to take full advantage of all Agoo's
features some configuration changes can be made. Those changes allow Agoo to
circumvent the default Rails and Rack behavior.

### Rack and Rails

Both Rack and Rails with Rack expect to handle all HTTP requests. The `call`
method is expected to handle serving static assets as well as Ruby
processing. Usually a stack of middleware is used for processing.

#### Static Assets

The Rack approach is fine except that using a middleware stack to handle
requests for static assets is expensive. Fortunately Rack expects static
assests to be in the `./public` directory by convention. Rails deviates a bit
and sets up an alias for a set of directories so that a request to `/assets`
gets resolved to a lookup in one of several directories. The first match is
then returned. Its a nice feature. The `/assets` directory group is described
in the `Rails.configuration.assets.paths` variable.

#### De-multiplexing Handlers

Rack does the de-multiplexing of HTTP requests in Ruby. That means a request
on `/hello` and one on `/goodbye` hit a Ruby call to decide that Ruby handler
should take care of the request. Many other web servers handle that type of
de-multiplexing before passing control to the handlers.

### Agoo to the Rescue

Agoo has some options to work around the Rack and Rails defaults while
preserving the overall behavior.

#### Static Assets

The Ruby middleware stack isn't really needed to serve static assets. By
default when `rackup` is used with Rails requests on `/assets` is resolved
into a check of the value of `Rails.configuration.assets.paths` at start up
just as Rails does. The difference is that request is handled completely by
Agoo and never touches Ruby code. This results in several orders of magnitude
better performance.

For straight Rack, when `rackup` is called or when ever the
`Rack::Handler::Agoo` handler is used the `./public` directory is checked
before calling the Ruby handle `call` method. This is controlled by the `rmux`
option which default to true. Calling `rackup` with an argument of `-O
root_first=false` turns that option off.

To set up path groups like the `/assets` group that Rails uses a call to
`Agoo::Server.path_group` is made.

#### De-multiplexing Handlers

In any non-trivial application multiple Ruby classes are used to process
requests. Maybe `/users` is handle by one class and `/articles` is handled by
another. In Rails those classes are buried deep within Rails. You can still
bypass Rails and Rack middleware if you want to for ancillary handlers. If
building an application directly using the Rack spec and want take a more
direct approach to invoking handlers then the Agoo de-multiplexing options
make the application request handling much simpler.

Setting up handlers for specific paths uses the `Agoo::Server.handle`
method. When a request arrives each handler path is checked in the order they
were registered. As and example, if these two calls were made.

```ruby
Agoo::Server.handle(nil, '/one', FirstHandler)
Agoo::Server.handle(nil, '/**', CatchAll)
```

If a request arrive on `/one` the `FirstHandler` will be called. If on `/two`
the `CatchAll` handler will be called.

The handlers paths can be set up using `rackup` as well. Since `rackup`
already registers `/**` to the Rack handler it is the `CatchAll` in the above
example. To register handlers before that the `-O /one=FirstHandle` option is
added to the `rackup` arguments.

### Putting it all Together

Using both static asset configurations and de-multiplexing will give the best
performance. Couple that with Agoo's cluster option on a multithreaded machine
give an even bigger win.

### Examples

For a `rackup` setup with static assets in `./assets` and a handler for
`/help` that uses the `HelpMe` class.

`rackup -d ./assets -O /help=HelpMe`

For a Ruby startup using `ruby app.rb` add these to the app to use `./public`
for static assets but add a group that handles assets at `./assets` and also
`./public/assets`.

```ruby
Agoo::Server.init(9292, `./public`, root_first: true)
Agoo::Server.path_group('./assets`, [`./assets`, `./public/assets'])
```
