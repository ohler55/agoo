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

What you should see is a faster Rails. Request to something like
`http://localhost:9292/users/1` should be twice as fast and asset fetches
should be 2000 times faster. Here are some benchmark runs using the
[perfer](https://github.com/ohler55/perfer) benchmark tool.

Lets first simulate 20 users accessing Rails at the same. Not a very heavy
load. We don't want to stress Rails too much.

##### With Rails and the default Puma server.

```
$ bin/perfer -p /users/1 127.0.0.1:9292  -t 2 -c 10 -b 1 -k -d 5 
Benchmarks for:
  URL:                127.0.0.1:9292/users/1
  Threads:            2
  Connections/thread: 10
  Duration:           5.7 seconds
  Keep-Alive:         true
Results:
  Throughput:         47 requests/second
  Latency:            405.269 +/-716.303 msecs (and stdev)
```

##### Now the same request with Agoo as webserver.

```
$ bin/perfer -p /users/1 127.0.0.1:9292  -t 2 -c 10 -b 1 -k -d 5
Benchmarks for:
  URL:                127.0.0.1:9292/users/1
  Threads:            2
  Connections/thread: 10
  Duration:           5.2 seconds
  Keep-Alive:         true
Results:
  Throughput:         76 requests/second
  Latency:            254.692 +/-30.366 msecs (and stdev)
```

Okay, not that impressive. Only a 60% improvement but it is faster. Most of
the time is spent in the Ruby call stack which at times reaches a depth of
over 1500. Still it is an improvement. Normally a 60% improvement would be
awesome but check out the improvement with asset loading.

We bumped the number of users up to 40 and allowed for a backlog of 2 which
isn't much when loading assets.

##### Rails with the default Puma server loading a static asset.
```
$ bin/perfer -p /assets/users.scss 127.0.0.1:9292  -t 2 -c 20 -b 2 -k -d 5
Benchmarks for:
  URL:                127.0.0.1:9292/assets/users.scss
  Threads:            2
  Connections/thread: 20
  Duration:           2.9 seconds
  Keep-Alive:         true
Results:
  Throughput:         82 requests/second
  Latency:            905.159 +/-1751.599 msecs (and stdev)
```

Well, Rails with Puma handles almost twice as as many request as it does for
Ruby processing calls. Sadly 40 users seems to drop the response times to
almost a one second. Wow, only 40 users and one second resonse time just to
load a single CSS file.

##### Rails with Agoo loading a static asset.
```
$ bin/perfer -p /assets/users.scss 127.0.0.1:9292  -t 2 -c 20 -b 2 -k -d 5
Benchmarks for:
  URL:                127.0.0.1:9292/assets/users.scss
  Threads:            2
  Connections/thread: 20
  Duration:           5.0 seconds
  Keep-Alive:         true
Results:
  Throughput:         182374 requests/second
  Latency:            0.435 +/-0.086 msecs (and stdev)
```

Wow, more than 2000 times faster and the latency drops from 1 second to half a
millisecond. There is a reason for that. Agoo looks at the
`Rails.configuration.assets.paths` variable and sets up a bypass to load those
files directly using the same approach as Rails. Ruby is no longer has to deal
with basic file serving but can be relogated to taking care of business. It
also means tha no Ruby objects are created for serving static assets.

