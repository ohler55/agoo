# agoo

[![Build Status](https://img.shields.io/travis/ohler55/agoo/master.svg)](http://travis-ci.org/ohler55/agoo?branch=master)
[![Gem Version](https://badge.fury.io/rb/agoo.svg)](https://badge.fury.io/rb/agoo)
![Gem](https://img.shields.io/gem/dt/agoo.svg)

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
# To run this example type the following then go to a browser and enter a URL of localhost:6464/hello.
# ruby hello.rb
```

## Installation
```
gem install agoo
```

## What Is This?

Agoo is Japanese for a type of flying fish. This gem flies. It is a high
performance HTTP server that serves static resource at hundreds of thousands
of fetchs per second. A a simple hello world Ruby handler at over 100,000
requests per second on a desktop computer. That places Agoo at about 85 times
faster than Sinatra and 1000 times faster than Rails. In both cases the
latency was two orders of magnitude lower or more. Checkout the benchmarks on
<a href="http://opo.technology/benchmarks.html#web_benchmarks">OpO
benchmarks</a>. Note that the benchmarks had to use a C program called <a
href="https://github.com/ohler55/perfer">perfer</a> to hit the Agoo
limits. Ruby benchmarks driver could not push Agoo hard enough.

Agoo supports the [Ruby rack API](https://rack.github.io) which allows for the
use of rack compatible gems. Agoo also supports WebSockets and SSE.

## Releases

See [file:CHANGELOG.md](CHANGELOG.md)

Releases are made from the master branch. The default branch for checkout is
the develop branch. Pull requests should be made against the develop branch.

## Links

 - *Documentation*: http://rubydoc.info/gems/agoo

 - *GitHub* *repo*: https://github.com/ohler55/agoo

 - *RubyGems* *repo*: https://rubygems.org/gems/agoo

 - *WABuR* *repo*: https://github.com/ohler55/wabur has an option to use Agoo

 - *Perer* *repo*: https://github.com/ohler55/perfer

Follow [@peterohler on Twitter](http://twitter.com/#!/peterohler) for announcements and news about the Agoo gem.
