# agoo

[![Build Status](https://img.shields.io/travis/ohler55/agoo/master.svg)](http://travis-ci.org/ohler55/agoo?branch=master)
[![Gem Version](https://badge.fury.io/rb/agoo.svg)](https://badge.fury.io/rb/agoo)
![Gem](https://img.shields.io/gem/dt/agoo.svg) [![TideLift](https://tidelift.com/badges/github/ohler55/agoo)](https://tidelift.com/subscription/pkg/rubygems-agoo?utm_source=rubygems-agoo&utm_medium=referral&utm_campaign=readme)

A High Performance HTTP Server for Ruby

## Usage

```ruby
require 'agoo'

Agoo::Server.init(6464, 'root')

class MyHandler
  def call(req)
    [ 200, { }, [ "hello world" ] ]
  end
end

handler = MyHandler.new
Agoo::Server.handle(:GET, "/hello", handler)
Agoo::Server.start()

# To run this example type the following then go to a browser and enter a URL
# of localhost:6464/hello.
#
# ruby hello.rb
```

## Installation
```
gem install agoo
```

## What Is This?

Agoo is Japanese for a type of flying fish. This gem flies. It is a high
performance HTTP server that serves static resource at hundreds of thousands
of fetches per second. A simple hello world Ruby handler at over 100,000
requests per second on a desktop computer. That places Agoo at about 85 times
faster than Sinatra and 1000 times faster than Rails. In both cases the
latency was two orders of magnitude lower or more. Checkout the
[benchmarks](http://opo.technology/benchmarks.html#web_benchmarks). Note that
the benchmarks had to use a C program called
[Perfer](https://github.com/ohler55/perfer) to hit the Agoo limits. Ruby
benchmarks driver could not push Agoo hard enough.

Agoo supports the [Ruby rack API](https://rack.github.io) which allows for the
use of rack compatible gems such as Hanami and Rails. Agoo also supports WebSockets and SSE.

Agoo is not available on Windows.

## News

- Agoo takes first place as the highest throughput on [web-frameworks
  benchmarks](https://github.com/the-benchmarker/web-frameworks). Latency was
  not at the top but release 2.5.2 improves that. Look for an Agoo-C benchmark
  in the future.

- Clustered Agoo is ready. For slower application and a machine with multiple
  cores a significant improvement is performance is realized. The application
  must be stateless in that no data is shared between workers.

- WebSocket and SSE are supported and a PR has been submitted to updated the
  Rack spec. Go over to the [proposed Rack
  extension](https://github.com/rack/rack/pull/1272) and give it a look and a
  thumbs-up or heart if you like it.

- Agoo now serves Rails static assets more than 8000 times faster than the
  default Puma. Thats right, [8000 times faster](misc/rails.md).

## Releases

See [file:CHANGELOG.md](CHANGELOG.md)

Releases are made from the master branch. The default branch for checkout is
the develop branch. Pull requests should be made against the develop branch.

## Support

[Get supported Agoo with a Tidelift Subscription.](https://tidelift.com/subscription/pkg/rubygems-agoo?utm_source=rubygems-agoo&utm_medium=referral&utm_campaign=readme)

## Links

 - *Documentation*: http://rubydoc.info/gems/agoo or http://www.ohler.com/agoo/doc/index.html

 - *GitHub* *repo*: https://github.com/ohler55/agoo

 - *RubyGems* *repo*: https://rubygems.org/gems/agoo

 - *WABuR* *repo*: https://github.com/ohler55/wabur has an option to use Agoo

 - *Perfer* *repo*: https://github.com/ohler55/perfer

Follow [@peterohler on Twitter](http://twitter.com/#!/peterohler) for announcements and news about the Agoo gem.
