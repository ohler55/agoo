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

  def test_rack
    begin
      server = Agoo::Server.new(6467, 'root',
				pedantic: false,
				log_dir: '',
				thread_count: 1,
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

      handler = TellMeHandler.new
      server.handle(:GET, "/tellme", handler)
      server.handle(:POST, "/makeme", handler)
      server.handle(:PUT, "/makeme", handler)

      server.start()

      eval_test
      post_test
      put_test
    ensure
      server.shutdown
    end
  end
  
  def eval_test
    uri = URI('http://localhost:6467/tellme?a=1')
    req = Net::HTTP::Get.new(uri)
    # Set the headers the way we want them.
    req['Accept-Encoding'] = '*'
    req['Accept'] = 'application/json'
    req['User-Agent'] = 'Ruby'

    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    obj = Oj.load(content, mode: :strict)

    expect = {
      "HTTP_Accept" => "application/json",
      "HTTP_Accept-Encoding" => "*",
      "HTTP_User-Agent" => "Ruby",
      "PATH_INFO" => "",
      "QUERY_STRING" => "a=1",
      "REQUEST_METHOD" => "GET",
      "SCRIPT_NAME" => "/tellme",
      "SERVER_NAME" => "localhost",
      "SERVER_PORT" => "6467",
      "rack.errors" => nil,
      "rack.input" => nil,
      "rack.multiprocess" => false,
      "rack.multithread" => false,
      "rack.run_once" => false,
      "rack.url_scheme" => "http",
      "rack.version" => "1.3",
    }
    expect.each_pair { |k,v|
      if v.nil?
	assert_nil(obj[k], k)
      else
	assert_equal(v, obj[k], k)
      end
    }
  end

  def post_test
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
  
  def put_test
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
