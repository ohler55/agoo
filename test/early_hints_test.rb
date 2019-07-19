#!/usr/bin/env ruby

$: << File.dirname(__FILE__)
$root_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
%w(lib ext).each do |dir|
  $: << File.join($root_dir, dir)
end

require 'minitest'
require 'minitest/autorun'
require 'net/http'

require 'agoo'

class EarlyHandlerTest < Minitest::Test
  @@server_started = false

  class CallHandler
    def self.call(req)
      headers = { 'Content-Type' => 'text/plain' }
      eh = req['early_hints']
      unless eh.nil?
	headers['Link'] = "Link: </style.css>; rel=preload; as=style, </script.js>; rel=preload; as=script"
	eh.call([
		  ["Link", "</style.css>; rel=preload; as=style"],
		  ["Link", "</script.js>; rel=preload; as=script"],
		])
      end
      sleep(0.2)
      [ 200, headers, [ 'called' ]]
    end
  end

  def start_server
    Agoo::Log.configure(dir: '',
			console: true,
			classic: true,
			colorize: true,
			states: {
			  INFO: false,
			  DEBUG: false,
			  connect: false,
			  request: false,
			  response: false,
			  eval: true,
			})

    Agoo::Server.init(6473, 'root', thread_count: 1)

    Agoo::Server.handle(:GET, "/call", CallHandler)
    Agoo::Server.start()
    Agoo::Server::rack_early_hints(true)

    @@server_started = true
  end

  def setup
    unless @@server_started
      start_server
    end
  end

  Minitest.after_run {
    GC.start
    Agoo::shutdown
  }

  def test_rack_early_hint
    on = Agoo::Server::rack_early_hints(true)
    x = Agoo::Server::rack_early_hints(nil)
    off = Agoo::Server::rack_early_hints(false)
    Agoo::Server::rack_early_hints(true) # back to starting state
    assert_equal(true, on, 'after turning on')
    assert_equal(true, x, 'on query with nil')
    assert_equal(false, off, 'after turning off')
    assert_raises(ArgumentError) { Agoo::Server::rack_early_hints(2) }
  end

  def test_call
    res = long_request('http://localhost:6473/call')
    assert_equal(%|HTTP/1.1 103 Early Hints\r
Link: </style.css>; rel=preload; as=style\r
Link: </script.js>; rel=preload; as=script\r
\r
HTTP/1.1 200 OK\r
Content-Length: 6\r
Content-Type: text/plain\r
Link: Link: </style.css>; rel=preload; as=style, </script.js>; rel=preload; as=script\r
\r
called|, res)
  end

  def long_request(uri)
    u = URI(uri)
    res = ''
    TCPSocket.open(u.hostname, u.port) { |s|
      s.send("GET #{u.path} HTTP/1.1\r\n\r\n", 0)
      sleep(0.3) # wait for results to be collected
      res = s.recv(1000)
    }
    res
  end

end
