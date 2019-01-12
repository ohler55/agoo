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

class StaticTest < Minitest::Test
  @@server_started = false

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

    Agoo::Server.init(6469, 'root', thread_count: 1)
    Agoo::Server.add_mime('odd', 'text/odd')
    Agoo::Server.start()

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

  def test_fetch_index
    uri = URI('http://localhost:6469/index.html')
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

  def test_mime
    uri = URI('http://localhost:6469/odd.odd')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal('text/odd', res['Content-Type'])
  end

  def test_fetch_auto_index
    uri = URI('http://localhost:6469/')
    content = Net::HTTP.get(uri)
    expect = %|<!DOCTYPE html>
<html>
  <head><title>Agoo Test</title></head>
  <body>Agoo</body>
</html>
|
    assert_equal(expect, content)
  end

  def test_fetch_nested
    uri = URI('http://localhost:6469/nest/something.txt')
    content = Net::HTTP.get(uri)
    assert_equal('Just some text.
', content)
  end

  def test_fetch_not_found
    uri = URI('http://localhost:6469/bad.html')
    res = Net::HTTP.get_response(uri)
    assert_equal("404", res.code)
  end

end
