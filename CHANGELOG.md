# Changelog

All changes to the Agoo gem are documented here. Releases follow semantic versioning.

## [2.15.2] - 2022-06-21

### Fixed

- Request no longer crashes when `#env` is not called before `#set`.

## [2.15.1] - 2022-06-18

### Added

- Introspection can now be disabled with the `hide_schema` option to the Agoo server.

- If an exception raised from a GraphQL callback responds to `code`
  that code will be used as the HTTP status.

- Added missing ability to set or add elements of a request argument
  on GraphQL callback methods.

## [2.15.0] - 2022-05-20

### Added

- Support added for PATCH.

- A `:hide_schema` option has been added to show the graphql/schema as
  not found unless added by with the handle method of the server.

- Raising an exception that responds to `code` in a graphql resolve
  function will return that code as the HTTP status code.

## [2.14.3] - 2022-05-05

### Fixed
- Agoo now reports an error if the developer make the mistake of
  building a schema that loops back on itself too many times using
  fragments.

## [2.14.2] - 2022-02-22

### Fixed
- Invalid SDL now raises and exception instead of crashing.

## [2.14.1] - 2021-06-09

### Fixed
- Evaluating an empty GraphQL request with comments only no longer crashes.
- JSON parser bug fixed.

## [2.14.0] - 2020-11-07

### Added

- REMOTE_ADDR element added to requests/env argument to `call()`.

- Added check for multiple Content-Length headers.

- Multiple occurrances of a header are now passed to the Rack `call()` method as an array.

## [2.13.0] - 2020-07-05

### Added

- Agoo::Server.use() added. It works similar to the Rack use() function.

### Fixed

- Header checks are now case insensitive to comply with RFC 7230.

## [2.12.3] - 2020-03-28

rb_rescue2 termination

### Fixed

- rb_rescue2 must be terminated by a (VALUE)0 and not simply 0.

## [2.12.2] - 2020-03-22

Stub generator

### Added

- GraphQL stub generator to generate Ruby class file corresponding to GraphQL schema type. The generator is `bin/agoo_stubs`.

## [2.12.1] - 2020-03-17

Check SO_REUSEPORT

### Fixed

- Verifiy SO_REUSEPORT is define for older OS versions.

## [2.12.0] - 2020-01-19

Request GraphQL access and GraphiQL support

### Added

- Optional addition of request information is now available in GraphQL resolve functions.

- Headers for GraphQL responses can now be set.

- Fixed incorrect values in introspection responses.

- Fixed JSON parser bug with null, true, and false.

## [2.11.7] - 2020-01-02

Ruby 2.7.0 cleanup

### Fixed

- .travis.yml cleanup (thanks waghanza).

- Compile warnings eliminated.

- Added `require 'stringio'` to agoo.rb (issue #80)

## [2.11.6] - 2019-12-15

Bug fixes

### Fixed

- Missing variable arguments no longer cause a crash.

- Eliminated the type cast warnings introduced by the Ruby 2.6.5 which uses stricter makefile settings.

## [2.11.5] - 2019-11-11

Alpine

### Fixed

- Added compiler directives to allow building on Alpine linux.

## [2.11.4] - 2019-11-10

GraphQL Introspection

### Fixed

- __Field deprecatedReason is no longer listed as 'reason'.

- Missing SCHEMA __Type kind added.

## [2.11.3] - 2019-11-02

Benchamark options

- An option for polling timeout added. Trade off CPU use with reponsiveness.

## [2.11.2] - 2019-10-20

Coerce improvements

### Fixed
- Coerce of `nil` to anything returns GraphQL `null`.

- Coerce String to defined scalar now succeeds.

- Handle comments in all locations.

## [2.11.1] - 2019-09-22

Race fix

### Fixed
- A race condion occurred with a slow Ruby response and a connection close. Fixed.

## [2.11.0] - 2019-09-15

TLS using OpenSSL

### Added
- TLS (SSL) support added.

## [2.10.0] - 2019-08-29

GraphQL subscriptions

### Fixed
- Rack multiple value header transformation changed to emit multiple entries instead of multiple values.

### Added
- GraphQL Subscriptions using Websockets and SSE are now supported.

## [2.9.0] - 2019-07-26

Early hints and sub-domains

### Added

- Support for early hints added. This includes a server option to
  activate and then a proposed `early_hints` member of the `env`
  argument to the Rack `#call` method.

- Sub-domains based on the host header in an HTTP request can now
  be used to set up multiple root directories.

## [2.8.4] - 2019-06-14

GraphQL with Rack

### Fixed
- Fixed an issue with using GraphQL with Rack. Added a Rack GraphQL example in
  example/graphql/config.ru.

## [2.8.3] - 2019-04-04

Description parsing fix.

### Fixed
- Fixed a description parse bug that would fail to ignore whitespace after a description on some quoted descriptions.

## [2.8.2] - 2019-03-08

Rack compatibility improvement.

### Added
- Rack builder wrapper added.

### Fixed
- Add miss `close` method to `Agoo::ErrorStream`.

## [2.8.1] - 2019-03-01

Add missing options.

### Added
- Add missing options to `bin/agoo`.
- Add missing options to `rack::Handler::Agoo`.
- `bin/agoo` now picks up `config.ru` or `config.rb` if no other files are specified.

## [2.8.0] - 2019-02-19

Extend GraphQL

### Added
- Add support for the Graphql `extend` functionality.
- Add the ID base scalar type.

### Fixed
- Use `schema` SDL element instead of `type schema`.

## [2.7.0] - 2019-01-29

Static asset header rules functionality added.

### Added
- `Agoo::Server.header_rule()` added.

### Fixed
- Assorted code cleanup.

## [2.6.1] - 2019-01-20

Ruby 2.6.0 compatibility release.

### Added
- Registering the GraphQL root object fixed for 2.6.0.

## [2.6.0] - 2019-01-20

GraphQL supported with a simple, easy to use API. Existing API remain the same but a new Agoo::GraphQL module has been added along with supporting examples.

### Fixed
- Replaced the use of `gmtime` with an included function to assure support for dates before 1970.

## [2.5.7] - 2018-12-03

Bad release fix along with upload example.

### Fixed
- Additional return value checking on `strdup`, `strndup`, and `fstat`.
- Fix double free when a connection times out.

## [2.5.6] - 2018-11-26

Even more cleanup and minor performance tweak.

### Added
- Compile option to use epoll instead of poll. (its slower)

### Fixed
- Changed the use of __ and _X in C code.
- Resuse of some memory for responses.

## [2.5.5] - 2018-11-12

More optimizations and some cleanup.

### Added
- New optional hint to the server that allows responses to `#call` to be cached.

### Fixed
- Change C header file guards. No change in behavior. Just code cleanup.
- Fixed change that slowed down the server.

## [2.5.4] - 2018-11-10

Third times a charm.

### Fixed
- Make sure response are sent when the request includes a close indicator.

## [2.5.3] - 2018-11-10

Bug fix.

### Fixed
- The latest changes introduced a bug where requests were lost. Fixed in this release.

## [2.5.2] - 2018-11-09

Latency improvements.

### Changed
- Allow empty pages to be returned.
- Included multiple read/write threads.

## [2.5.1] - 2018-08-30

Bug fix.

### Fixed
- Sending a POST request that was not handled and caused a segfault. Fixed.

## [2.5.0] - 2018-08-10

The binding release with multiple options for connecting to the server.

### Added
- Multiple connection bindings now supported
- Binding to Unix named sockets support added.
- IPv6 now supported.
- Connection address now supported to restrict connection requests.

## [2.4.0] - 2018-07-04

### Added
- Rack hijack now supported.
- Upgraded handler `on_error` now supported and called on Websocket or SSE errors.

## [2.3.0] - 2018-06-29

### Added
- Added an `env` method to the upgrade (Websocket and SSE) client.

### Fixed
- Fixed Websocket bug where a pong caused a hang on the socket.

## [2.2.2] - 2018-06-05

### Added
- Added optimization description in `misc/optimize.md`.

### Fixed
- Fixed `bin/agoo` which had become out of date.
- Fixed static file asset page caching bug.

## [2.2.1] - 2018-05-31

### Fixed
- Corrected header bug where a `:` in the value would cause an incorrect header key and value.
- Improved idle CPU use.

## [2.2.0] - 2018-05-30

### Added
- Clustering now supported with forked workers making Agoo even faster.
- Changed addition header keys to be all uppercase and also replace `-` with `_` to match Rack unit tests instead of the spec.

## [2.1.3] - 2018-05-16

### Added
- Optimized `rackup` to look for files in the root directory before calling rackup as the default. The root is now set to `./public` instead of `.`. The `:rmux` (rack multiplex) turns that off.

## [2.1.2] - 2018-05-15

### Added
- Added `#open?` method to the Upgraded (connection client) class.
- Slight improvement performcance serving static assets for Rails.

## [2.1.1] - 2018-05-11

### Added
- Subject can now be Symbols or any other object that responds top `#to_s`.
- Super fast asset loading for Rails and Rack using `Agoo::Server.path_group`

### Fixed
- Fixed bug where publishes from the `#on_open` callback broke the connection.

## [2.1.0] - 2018-05-10

### Added
- Publish and subscribe to WebSocket and SSE connections now available. An example in the push subdirectory demonstrates how it works.
- Slight performance boost.

### Changed
- This is a minor release even though the API has changed. The changed API is the one for Rack based WebSocket and SSE connection upgrades. The PR for the spec addition is currently stalled but some suggestions for a stateless API are implemented in this release. The proposed Rack SPEC is [here](misc/SPEC). The PR is [here](https://github.com/rack/rack/pull/1272)

## [2.0.5] - 2018-05-06

### Changed
- Changed to putting all the path on the `REQUEST_PATH` variable instead of the `SCRIPT_NAME` to accomodate Rails which only uses the `REQUEST_PATH`.
- Duplicated all `HTTP_` variables to use an all upper case key in addition to the Rack spec simple concatenation to make Rails middleware work. There seems to be an undocumented agreement that all keys will be uppercase.

## [2.0.4] - 2018-05-06

### Fixed
- Fix allocation bug.

## [2.0.3] - 2018-05-05

### Added
- Allow X-XSS-Protection and only check headers in pendantic mode.

## [2.0.2] - 2018-05-04

### Fixed
- Fixed compiler issues for different OSs and compilers.
- More tolerant of `SERVER_NAME` capture from HTTP headers.

## [2.0.1] - 2018-05-01

### Fixed
- Allow compilation on older macOS compilers.

## [2.0.0] - 2018-04-30

### Added
- WebSocket and SSE support added.
- The API moved to a global server approach to support extended
  handlers for WebSocket and SSE connections.

## [1.2.2] - 2018-03-26

### Added
- rackup option -s now works with 'agoo' as well as with 'Agoo'.

### Fixed
- Fixed a memory leak.

## [1.2.1] - 2018-03-16

### Fixed
- Improved Rack handling. Rack version is an array as it was
  supposed to be. Hacked around Rack's handling of HEAD requests so
  that the content length can be returned more easily by the app.

## [1.2.0] - 2018-03-01

### Added
- Added Rack::Handler::Agoo for a rackup like use of Agoo.
- Examples provided.
- Default thread count set to zero.

## [1.1.2] - 2018-02-25

### Fixed
- Fixed pipelining to handle extreme rates.

## [1.1.1] - 2018-02-23

### Added
- An `agoo` binary was added to run an Agoo server from the command line.

## [1.1.0] - 2018-02-11

### Added
- New mime types can now be added
- Added support for rack.logger

## [1.0.0] - 2018-02-06

### Added
- Add not found handler support.
- Fixed segfault due to GC of handlers.

## [0.9.1] - 2018-01-28

Updated to get travis working correctly.

## [0.9.0] - 2018-01-28

Initial release.
