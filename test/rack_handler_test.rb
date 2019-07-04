#!/usr/bin/env ruby

$: << File.dirname(__FILE__)
$root_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
%w(lib ext).each do |dir|
  $: << File.join($root_dir, dir)
end

require 'minitest'
require 'minitest/autorun'
require 'net/http'

require 'oj'

require 'agoo'

class RackHandlerTest < Minitest::Test
  @@server_started = false

  class TellMeHandler
    def initialize
    end

    def call(req)
      if 'GET' == req['REQUEST_METHOD']
	[ 200,
	  { 'Content-Type' => 'application/json' },
	  [ Oj.dump(req.to_h, mode: :null) ]
	]
      elsif 'POST' == req['REQUEST_METHOD']
	[ 204, { }, [] ]
      elsif 'PUT' == req['REQUEST_METHOD']
	[ 201,
	  { },
	  [ req['rack.input'].read ]
	]
      else
	[ 404, {}, []]
      end

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

    Agoo::Server.init(6467, 'root', thread_count: 1)

    handler = TellMeHandler.new
    Agoo::Server.handle(:GET, "/tellme", handler)
    Agoo::Server.handle(:POST, "/makeme", handler)
    Agoo::Server.handle(:PUT, "/makeme", handler)

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

  def test_rack_early_hint
    on = Agoo::Server::rack_early_hints(true)
    x = Agoo::Server::rack_early_hints(nil)
    off = Agoo::Server::rack_early_hints(false)
    assert_equal(true, on, 'after turning on')
    assert_equal(true, x, 'on query with nil')
    assert_equal(false, off, 'after turning off')
    assert_raises(ArgumentError) { Agoo::Server::rack_early_hints(2) }
  end

  def test_eval
    uri = URI('http://localhost:6467/tellme?a=1')
    req = Net::HTTP::Get.new(uri)
    # Set the headers the way we want them.
    req['Accept-Encoding'] = '*'
    req['Accept'] = 'application/json'
    req['User-Agent'] = 'Ruby'
    req['Host'] = 'localhost:6467'

    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    obj = Oj.load(content, mode: :strict)
    expect = {
      "HTTP_ACCEPT" => "application/json",
      "HTTP_ACCEPT_ENCODING" => "*",
      "HTTP_USER_AGENT" => "Ruby",
      "HTTP_HOST" => "localhost:6467",
      "PATH_INFO" => "/tellme",
      "QUERY_STRING" => "a=1",
      "REQUEST_METHOD" => "GET",
      "SCRIPT_NAME" => "",
      "SERVER_NAME" => "localhost",
      "SERVER_PORT" => "6467",
      "rack.errors" => nil,
      "rack.input" => nil,
      "rack.multiprocess" => false,
      "rack.multithread" => false,
      "rack.run_once" => false,
      "rack.url_scheme" => "http",
      "rack.version" => [1, 3],
    }
    expect.each_pair { |k,v|
      if v.nil?
	assert_nil(obj[k], k)
      else
	assert_equal(v, obj[k], k)
      end
    }
  end

  def test_post
    uri = URI('http://localhost:6467/makeme')
    req = Net::HTTP::Post.new(uri)
    # Set the headers the way we want them.
    req['Accept-Encoding'] = '*'
    req['Accept'] = 'application/json'
    req['User-Agent'] = 'Ruby'

    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal(Net::HTTPNoContent, res.class)
  end

  def test_put
    uri = URI('http://localhost:6467/makeme')
    req = Net::HTTP::Put.new(uri)
    # Set the headers the way we want them.
    req['Accept-Encoding'] = '*'
    req['Accept'] = 'application/json'
    req['User-Agent'] = 'Ruby'
    req.body = 'hello'

    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal(Net::HTTPCreated, res.class)
    assert_equal('hello', res.body)
  end

end
