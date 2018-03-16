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

  # Run all the tests in one test to avoid creating the server multiple times.
  def test_static
    begin
      server = Agoo::Server.new(6466, 'root',
				thread_count: 1,
				log_dir: '',
				log_console: true,
				log_classic: true,
				log_colorize: true,
				log_states: {
				  INFO: false,
				  DEBUG: false,
				  connect: false,
				  request: false,
				  response: false,
				  eval: true,
				})
      server.add_mime('odd', 'text/odd')
      server.start()
      fetch_index_test
      mime_test
      fetch_auto_index_test
      fetch_nested_test
      fetch_not_found_test
    ensure
      server.shutdown
    end
  end


  def fetch_index_test
    uri = URI('http://localhost:6466/index.html')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal('text/html', res['Content-Type'])
    expect = %|<!DOCTYPE html>
<html>
  <head><title>Agoo Test</title></head>
  <body>Agoo</body>
</html>
|
    assert_equal(expect, content)
  end

  def mime_test
    uri = URI('http://localhost:6466/odd.odd')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal('text/odd', res['Content-Type'])
  end

  def fetch_auto_index_test
    uri = URI('http://localhost:6466/')
    content = Net::HTTP.get(uri)
    expect = %|<!DOCTYPE html>
<html>
  <head><title>Agoo Test</title></head>
  <body>Agoo</body>
</html>
|
    assert_equal(expect, content)
  end

  def fetch_nested_test
    uri = URI('http://localhost:6466/nest/something.txt')
    content = Net::HTTP.get(uri)
    assert_equal('Just some text.
', content)
  end

  def fetch_not_found_test
    uri = URI('http://localhost:6466/bad.html')
    res = Net::HTTP.get_response(uri)
    assert_equal("404", res.code)
  end

end
