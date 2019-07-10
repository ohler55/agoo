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
      eh = req['early_hints']
      unless eh.nil?
	eh.call([
		  ["link", "</style.css>; rel=preload; as=style"],
		  ["link", "</script.js>; rel=preload; as=script"],
		])
      end
      sleep(0.2)
      [ 200,
	{ 'Content-Type' => 'text/plain' },
	[ 'called' ]
      ]
    end
  end

  class PushHandler
    def self.call(req)
      eh = req['early_hints']
      unless eh.nil?
	eh.push({
                  '/style.css' => { preload: true, rel: 'style' },
                  '/scrips.js' => { preload: true, rel: 'script' },
                })
      end
      sleep(0.2)
      [ 200,
	{ 'Content-Type' => 'text/plain' },
	[ 'pushed' ]
      ]
    end
  end

  def start_server
    Agoo::Log.configure(dir: '',
			console: true,
			classic: true,
			colorize: true,
			states: {
			  INFO: true,
			  DEBUG: false,
			  connect: false,
			  request: true,
			  response: true,
			  eval: true,
			})

    Agoo::Server.init(6473, 'root', thread_count: 1)

    Agoo::Server.handle(:GET, "/call", CallHandler)
    Agoo::Server.handle(:GET, "/push", PushHandler)
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
    puts "*** call: #{res}"
    assert_equal('called', res[200])
  end

  def test_push
    res = long_request('http://localhost:6473/push')
    puts "*** push: #{res}"
    assert_equal('pushed', res[200])
  end

  def long_request(uri)
    u = URI(uri)
    res = ''
    TCPSocket.open(u.hostname, u.port) { |s|
      puts "*** path: #{u.path}"
      # TBD handle minimal request
      s.send("GET #{u.path} HTTP/1.0\r\n\r\n", 0)
      res = s.recv(1000)
    }
    puts "*** res: #{res}"

    # TBD parse result with headers, code, and body

    {
      200 => res
    }
  end

end
