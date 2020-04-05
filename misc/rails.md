# Rails with Agoo

Agoo gives Rails a performance boost. It's easy to use Agoo with Rails. Start
with a Rails project. Something simple.

```
$ rails new blog
$ rails g scaffold User
$ rails db:migrate
```

Now run it with Agoo.

```
$ rackup -r agoo -s agoo
```

Or for an extra boost on a machine with multiple cores fire up Agoo with
multiple workers. The optimium seems to be about one worker per hyperthread or
two per core.

```
$ rackup -r agoo -s agoo -O wc=12
```

What you should see is a faster Rails. Requests to something like
`http://localhost:9292/users/1` should be several times faster and asset
fetches should be 8000 times faster. Here are some benchmark runs using the
[perfer](https://github.com/ohler55/perfer) benchmark tool.

It is easy to push Rails too hard and watch the latency climb. To avoid that
the number of open connections use is adjusted according to how well the
server and Rails can handle the load. A balance where the latency is as close
to the best attainable is used and the throughput is maximized for that
latency is used in all cases.

All benchmarks were run on a 6 core i7-8700 system with 16GB of memory. The
driver, `perfer` was run on a separate machine to allow Rails and the web
server full use of the benchmark machine.

### With Rails managed objects

For the benchmarks a blank `User` object is created using a browser. The a
fetch is performed repeatedly on that object.

##### With Rails and the default Puma server.

```
$ perfer -t 1 -c 1 -b 1 192.168.1.11:9292 -p /users/1 -d 10
Benchmarks for:
  URL:                192.168.1.11:9292/users/1
  Threads:            1
  Connections/thread: 1
  Duration:           10.0 seconds
  Keep-Alive:         false
Results:
  Throughput:         41 requests/second
  Latency:            23.612 +/-5.111 msecs (and stdev)
```

##### Now the same request with Agoo in non-clustered mode.

```
$ perfer -t 2 -k -c 4 -b 1 192.168.1.11:9292 -p /users/1 -d 10
Benchmarks for:
  URL:                192.168.1.11:9292/users/1
  Threads:            2
  Connections/thread: 4
  Duration:           5.0 seconds
  Keep-Alive:         true
Results:
  Throughput:         279 requests/second
  Latency:            28.608 +/-3.497 msecs (and stdev)
```

Agoo was able to handle 8 concurrent connections and still maintain the
latency target of under 30 millisecond. Throughput with Agoo was also 7 times
greater. Most of the time spent with both Agoo and Puma is most likely in
Rails and Rackup so lets try Agoo in cluster mode with the application is
stateless.

##### Now the same request with Agoo in clustered mode.

```
$ perfer -t 2 -k -c 20 -b 1 192.168.1.11:9292 -p /users/1 -d 10
Benchmarks for:
  URL:                192.168.1.11:9292/users/1
  Threads:            2
  Connections/thread: 20
  Duration:           5.0 seconds
  Keep-Alive:         true
Results:
  Throughput:         1476 requests/second
  Latency:            27.051 +/-11.107 msecs (and stdev)
```

Another 5x boost in throughput. That make Agoo in cluster mode 36 times faster
than the default Puma. In all fairness, Puma can be put into clustered mode as
well using a custom configuration file. If anyone would like to provide
formula for running Puma at optimum please let me know or create a PR for how
to run it more effectively.

### Fetching static assets

Rails likes to be in charge and is responsible for serving static assets. Some
servers such as Agoo offer an option to bypass Rails handling of static
assets. Allowing static assets to be loaded directly means CSS, HTML, images,
Javascript, and other static files load quickly so that web sites are more
snappy even when more than a handful of users are connected.

##### Rails with the default Puma server loading a static asset.
```
$ perfer -t 2 -k -c 1 -b 1 192.168.1.11:9292 -p /robots.txt -d 10
Benchmarks for:
  URL:                192.168.1.11:9292/robots.txt
  Threads:            2
  Connections/thread: 1
  Duration:           10.0 seconds
  Keep-Alive:         true
Results:
  Throughput:         79 requests/second
  Latency:            25.027 +/-7.800 msecs (and stdev)
```

Well, Rails with Puma handles almost twice as as many request as it does for
Ruby processing of managed object calls. Note that Puma was able to handle 2
concurrent connections without degradation of the latency.

##### Rails with Agoo loading a static asset in non-clustered mode.
```
$ perfer -t 2 -k -c 40 -b 2 192.168.1.11:9292 -p /robots.txt -d 10
Benchmarks for:
  URL:                192.168.1.11:9292/robots.txt
  Threads:            2
  Connections/thread: 40
  Duration:           5.0 seconds
  Keep-Alive:         true
Results:
  Throughput:         500527 requests/second
  Latency:            0.292 +/-0.061 msecs (and stdev)
```

Wow, more than 6000 times faster and the latency drops from 25 milliseconds to
a fraction of a millisecond. There is a reason for that. Agoo looks at the
`Rails.configuration.assets.paths` variable and sets up a bypass to load those
files directly using the same approach as Rails. Ruby is no longer has to deal
with basic file serving but can be relogated to taking care of business. It
also means tha no Ruby objects are created for serving static assets.

Now how about Agoo in clustered mode.

##### Rails with Agoo loading a static asset in clustered mode.
```
$ perfer -t 2 -k -c 40 -b 2 192.168.1.11:9292 -p /robots.txt -d 10
Benchmarks for:
  URL:                192.168.1.11:9292/robots.txt
  Threads:            2
  Connections/thread: 40
  Duration:           5.0 seconds
  Keep-Alive:         true
Results:
  Throughput:         657777 requests/second
  Latency:            0.223 +/-0.073 msecs (and stdev)
```

A bit faster but not that much over the non-clustered Agoo. The limiting
factor with static assets is the network. Agoo handles 80 concurrent
connections at the same latency as one.

### Summary

Benchmarks are not everything but if they are an important consideration when
selecting a web server for Rails or Rack then Agoo clearly has an advantage
over the Rack and Rails default.
