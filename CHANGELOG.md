# CHANGELOG

### 2.8.1 - 2019-02-TBD

Add missing options.

- Add missing options to `bin/agoo`.

- Add missing options to `rack::Handler::Agoo`.

- `bin/agoo` now picks up `config.ru` or `config.rb` if no other files are specified.

### 2.8.0 - 2019-02-19

Extend GraphQL

- Add support for the Graphql `extend` functionality.

- Add the ID base scalar type.

- Use `schema` SDL element instead of `type schema`.

### 2.7.0 - 2019-01-29

Static asset header rules functionality added.

- `Agoo::Server.header_rule()` added.

- Assorted code cleanup.

### 2.6.1 - 2019-01-20

Ruby 2.6.0 compatibility release.

- Registering the GraphQL root object fixed for 2.6.0.

### 2.6.0 - 2019-01-20

GraphQL supported with a simple, easy to use API. Existing API remain the same but a new Agoo::GraphQL module has been added along with supporting examples.

- Replaced the use of `gmtime` with an included function to assure support for dates before 1970.

### 2.5.7 - 2018-12-03

Bad release fix along with upload example.

- Additional return value checking on `strdup`, `strndup`, and `fstat`.

- Fix double free when a connection times out.

### 2.5.6 - 2018-11-26

Even more cleanup and minor performance tweak.

- Changed the use of __ and _X in C code.

- Resuse of some memory for responses.

- Compile option to use epoll instead of poll. (its slower)

### 2.5.5 - 2018-11-12

More optimizations and some cleanup.

- New optional hint to the server that allows responses to `#call` to be cached.

- Change C header file guards. No change in behavior. Just code cleanup.

- Fixed change that slowed down the server.

### 2.5.4 - 2018-11-10

Third times a charm.

- Make sure response are sent when the request includes a close indicator.

### 2.5.3 - 2018-11-10

Bug fix.

- The latest changes introduced a bug where requests were lost. Fixed in this release.

### 2.5.2 - 2018-11-09

Latency improvements.

- Allow empty pages to be returned.

- Included multiple read/write threads.

### 2.5.1 - 2018-08-30

Bug fix.

- Sending a POST request that was not handled and caused a segfault. Fixed.

### 2.5.0 - 2018-08-10

The binding release with multiple options for connecting to the server.

- Multiple connection bindings now supported

- Binding to Unix named sockets support added.

- IPv6 now supported.

- Connection address now supported to restrict connection requests.

### 2.4.0 - 2018-07-04

- Rack hijack now supported.

- Upgraded handler `on_error` now supported and called on Websocket or SSE errors.

### 2.3.0 - 2018-06-29

- Added an `env` method to the upgrade (Websocket and SSE) client.

- Fixed Websocket bug where a pong caused a hang on the socket.

### 2.2.2 - 2018-06-05

- Fixed `bin/agoo` which had become out of date.

- Added optimization description in `misc/optimize.md`.

- Fixed static file asset page caching bug.

### 2.2.1 - 2018-05-31

- Corrected header bug where a `:` in the value would cause an incorrect header key and value.

- Improved idle CPU use.

### 2.2.0 - 2018-05-30

- Clustering now supported with forked workers making Agoo even faster.

- Changed addition header keys to be all uppercase and also replace `-` with `_` to match Rack unit tests instead of the spec.

### 2.1.3 - 2018-05-16

- Optimized `rackup` to look for files in the root directory before calling rackup as the default. The root is now set to `./public` instead of `.`. The `:rmux` (rack multiplex) turns that off.

### 2.1.2 - 2018-05-15

- Added `#open?` method to the Upgraded (connection client) class.

- Slight improvement performcance serving static assets for Rails.

### 2.1.1 - 2018-05-11

- Subject can now be Symbols or any other object that responds top `#to_s`.

- Fixed bug where publishes from the `#on_open` callback broke the connection.

- Super fast asset loading for Rails and Rack using `Agoo::Server.path_group`

### 2.1.0 - 2018-05-10

- This is a minor release even though the API has changed. The changed API is the one for Rack based WebSocket and SSE connection upgrades. The PR for the spec addition is currently stalled but some suggestions for a stateless API are implemented in this release. The proposed Rack SPEC is [here](misc/SPEC). The PR is [here](https://github.com/rack/rack/pull/1272)

- Publish and subscribe to WebSocket and SSE connections now available. An example in the push subdirectory demonstrates how it works.

- Slight performance boost.

### 2.0.5 - 2018-05-06

- Changed to putting all the path on the `REQUEST_PATH` variable instead of the `SCRIPT_NAME` to accomodate Rails which only uses the `REQUEST_PATH`.

- Duplicated all `HTTP_` variables to use an all upper case key in addition to the Rack spec simple concatenation to make Rails middleware work. There seems to be an undocumented agreement that all keys will be uppercase.

### 2.0.4 - 2018-05-06

- Fix allocation bug.

### 2.0.3 - 2018-05-05

- Allow X-XSS-Protection and only check headers in pendantic mode.

### 2.0.2 - 2018-05-04

- Fixed compiler issues for different OSs and compilers.

- More tolerant of `SERVER_NAME` capture from HTTP headers.

### 2.0.1 - 2018-05-01

- Allow compilation on older macOS compilers.

### 2.0.0 - 2018-04-30

- WebSocket and SSE support added.

- The API moved to a global server approach to support extended
  handlers for WebSocket and SSE connections.

### 1.2.2 - 2018-03-26

- rackup option -s now works with 'agoo' as well as with 'Agoo'.

- Fixed a memory leak.

### 1.2.1 - 2018-03-16

- Improved Rack handling. Rack version is an array as it was
  supposed to be. Hacked around Rack's handling of HEAD requests so
  that the content length can be returned more easily by the app.

### 1.2.0 - 2018-03-01

- Added Rack::Handler::Agoo for a rackup like use of Agoo.

- Examples provided.

- Default thread count set to zero.

### 1.1.2 - 2018-02-25

- Fixed pipelining to handle extreme rates.

### 1.1.1 - 2018-02-23

- An `agoo` binary was added to run an Agoo server from the command line.

### 1.1.0 - 2018-02-11

- New mime types can now be added
- Added support for rack.logger

### 1.0.0 - 2018-02-06

- Add not found handler support.
- Fixed segfault due to GC of handlers.

### 0.9.1 - 2018-01-28

Updated to get travis working correctly.

### 0.9.0 - 2018-01-28

Initial release.
