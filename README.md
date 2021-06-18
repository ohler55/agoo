# [![{}j](misc/agoo_128.svg)](http://www.ohler.com/agoo) Agoo

[![Build Status](https://img.shields.io/github/workflow/status/ohler55/agoo/CI?logo=github)](https://github.com/ohler55/agoo/actions/workflows/CI.yml)
[![Gem Version](https://badge.fury.io/rb/agoo.svg)](https://badge.fury.io/rb/agoo)
![Gem](https://img.shields.io/gem/dt/agoo.svg) [![TideLift](https://tidelift.com/badges/github/ohler55/agoo)](https://tidelift.com/subscription/pkg/rubygems-agoo?utm_source=rubygems-agoo&utm_medium=referral&utm_campaign=readme)

A High Performance HTTP Server for Ruby

## Usage

#### Rack

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

#### GraphQL

```ruby
require 'agoo'

class Query
  def hello
    'hello'
  end
end

class Schema
  attr_reader :query

  def initialize
    @query = Query.new()
  end
end

Agoo::Server.init(6464, 'root', thread_count: 1, graphql: '/graphql')
Agoo::Server.start()
Agoo::GraphQL.schema(Schema.new) {
  Agoo::GraphQL.load(%^type Query { hello: String }^)
}
sleep

# To run this GraphQL example type the following then go to a browser and enter
# a URL of localhost:6464/graphql?query={hello}
#
# ruby hello.rb
```

## Installation
```
gem install agoo
```

## Using agoo as server for rails

As agoo supports rack compatible apps you can use it for rails applications:

Add agoo to the Gemfile:

```
# Gemfile
gem 'agoo'
```

Install bundle:

```
$ bundle install
```

Start rails with agoo as server:

```
$ rails server -u agoo
```

Enjoy the increased performance!

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

- Version 2.11.0 supports GraphQL subscriptions. TLS (SSL,HTTPS)
  support added. Examples for both. Related, the
  [graphql-benchmark](https://github.com/the-benchmarker/graphql-benchmarks)
  repo was given to [the-benchmarker](https://github.com/the-benchmarker).

- Agoo has a new GraphQL module with a simple, easy to use
  API. Checkout the [hello](example/graphql/hello.rb) or
  [song](example/graphql/song.rb) examples.
  [An Instrumental Intro to GraphQL with Ruby](https://blog.appsignal.com/2019/01/29/graphql.html)
  is a walk through.

- Agoo takes first place as the highest throughput on [web-frameworks
  benchmarks](https://github.com/the-benchmarker/web-frameworks). Latency was
  not at the top but release 2.5.2 improves that. The Agoo-C benchmarks it at
  the top. The fastest web server across all languages.

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

[Get supported Agoo with a Tidelift Subscription.](https://tidelift.com/subscription/pkg/rubygems-agoo?utm_source=rubygems-agoo&utm_medium=referral&utm_campaign=readme) Security updates are [supported](https://tidelift.com/security).

## Links

 - *Documentation*: http://rubydoc.info/gems/agoo or http://www.ohler.com/agoo/doc/index.html

 - *GitHub* *repo*: https://github.com/ohler55/agoo

 - *RubyGems* *repo*: https://rubygems.org/gems/agoo

 - *WABuR* *repo*: https://github.com/ohler55/wabur has an option to use Agoo

 - *Perfer* *repo*: https://github.com/ohler55/perfer
