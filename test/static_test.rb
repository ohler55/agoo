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
      puts "fetch_index"
      fetch_index_test
      mime_test
      puts "fetch_auto"
      fetch_auto_index_test
      puts "fetch_nested"
      fetch_nested_test
      puts "fetch_not found"
      fetch_not_found_test
    ensure
      Agoo::shutdown
    end
  end


  def fetch_index_test
    uri = URI('http://localhost:6469/index.html')
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
    uri = URI('http://localhost:6469/odd.odd')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal('text/odd', res['Content-Type'])
  end

  def fetch_auto_index_test
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

  def fetch_nested_test
    uri = URI('http://localhost:6469/nest/something.txt')
    content = Net::HTTP.get(uri)
    assert_equal('Just some text.
', content)
  end

  def fetch_not_found_test
    uri = URI('http://localhost:6469/bad.html')
    res = Net::HTTP.get_response(uri)
    assert_equal("404", res.code)
  end

end
