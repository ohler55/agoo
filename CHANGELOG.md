# CHANGELOG

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
