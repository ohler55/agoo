#!/usr/bin/env ruby

$: << File.dirname(__FILE__)
$root_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
%w(lib ext).each do |dir|
  $: << File.join($root_dir, dir)
end

require 'minitest'
require 'minitest/autorun'
require 'net/http'
require 'socket'

require 'agoo'

class BindTest < Minitest::Test

  # Run all the tests in one test to avoid creating the server multiple times.
  def test_static
    begin
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

      addr = ''
      Socket.ip_address_list.each { |ai|
	if ai.ipv4? && '127.0.0.1' != ai.ip_address
	  addr = ai.ip_address
	  break
	end
      }
      addr6 = '::1'
      name = '/tmp/agoo_test.socket'
      Agoo::Server.init(6471, 'root', thread_count: 1,
			bind: ['http://127.0.0.1:6472',
			       "http://#{addr}:6473",
			       "http://[#{addr6}]:6474",
			       "unix://#{name}",
			      ])
      Agoo::Server.start()

      port_test
      localhost_test
      ipv4_test(addr)
      ipv6_test(addr6)
      restrict_test
      named_test(name)
    ensure
      Agoo::shutdown
    end
  end

  def port_test
    uri = URI('http://localhost:6471/index.html')
    request(uri)
  end

  def localhost_test
    uri = URI('http://localhost:6472/index.html')
    request(uri)
  end

  def ipv4_test(addr)
    uri = URI("http://#{addr}:6473/index.html")
    request(uri)
  end

  def ipv6_test(addr)
    uri = URI("http://[#{addr}]:6474/index.html")
    request(uri)
  end

  def restrict_test
    uri = URI("http://127.0.0.1:6473/index.html")
    assert_raises Exception do
      Net::HTTP.get(uri)
    end
  end

  def named_test(name)
    content = ''
    UNIXSocket.open(name) {|c|
      c.print(%|GET /index.html HTTP/1.1\r
Accept-Encoding: *\r
Accept: */*\r
User-Agent: Ruby\r
\r
|)
      rcnt = 0
      c.each_line { |line|
	rcnt = line.split(':')[1].to_i if line.start_with?("Content-Length: ")
	break if line.length <= 2 # \r\n
      }
      if 0 < rcnt
	content = c.read(rcnt)
      end
    }
    expect = %|<!DOCTYPE html>
<html>
  <head><title>Agoo Test</title></head>
  <body>Agoo</body>
</html>
|
    assert_equal(expect, content)
  end

  def request(uri)
    expect = %|<!DOCTYPE html>
<html>
  <head><title>Agoo Test</title></head>
  <body>Agoo</body>
</html>
|
    req = Net::HTTP::Get.new(uri)
    req['Accept-Encoding'] = '*'
    req['User-Agent'] = 'Ruby'
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal('text/html', res['Content-Type'])
    assert_equal(expect, content)
  end
  
end
