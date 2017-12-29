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

class FooHandlerTest < Minitest::Test

  class TellMeHandler
    def initialize
    end

    def call(req)
      puts "*** call #{req['REQUEST_METHOD']}"
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
	[ 404, {}, ['not found']]
      end
    end
  end

  def test_base_handler
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
      puts "*** test_rack after new"
      handler = TellMeHandler.new
      server.handle(:GET, "/tellme", handler)
      puts "*** test_rack before start"
      server.start()

      #sleep(100)
      puts "*** before eval_test"
      eval_test
      puts "*** after eval_test"
    ensure
      server.shutdown
      puts "*** after shutdown"
    end
  end
  
  def eval_test
    uri = URI('http://localhost:6467/tellme?a=1')
    req = Net::HTTP::Get.new(uri)
    # Set the headers the way we want them.
    req['Accept-Encoding'] = '*'
    req['Accept'] = 'application/json'
    req['User-Agent'] = 'Ruby'

    puts "*** http start"
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      puts "*** http request"
      h.request(req)
    }
    puts "*** http done"
    content = res.body
    puts "*** content #{content}"
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
      "rack.version" => "2.0.3",
    }
    expect.each_pair { |k,v|
      if v.nil?
	assert_nil(obj[k], k)
      else
	assert_equal(v, obj[k], k)
      end
    }
  end

end
