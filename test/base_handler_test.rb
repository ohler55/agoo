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

class BaseHandlerTest < Minitest::Test

  class TellMeHandler
    def initialize
    end

    def on_request(req, res)
      if 'GET' == req.request_method
	res.body = Oj.dump(req.to_h, mode: :null)
      elsif 'POST' == req.request_method
	res.code = 204
      elsif 'PUT' == req.request_method
	res.code = 201
	res.body = req.body
      end
    end
  end

  class WildHandler
    def initialize(name)
      @name = name
    end

    def on_request(req, res)
      res.body = "#{@name} - #{req.script_name}"
    end
  end

  def test_base_handler
    begin
      server = Agoo::Server.new(6470, 'root',
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
      server.handle(:GET, "/wild/*/one", WildHandler.new('one'))
      server.handle(:GET, "/wild/all/**", WildHandler.new('all'))
      server.start()

      #sleep(100)
      eval_test
      post_test
      put_test
      wild_one_test
      wild_all_test
    ensure
      server.shutdown
    end
  end
  
  def eval_test
    uri = URI('http://localhost:6470/tellme?a=1')
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
      "SERVER_PORT" => "6470",
      "rack.errors" => nil,
      "rack.input" => nil,
      "rack.multiprocess" => false,
      "rack.multithread" => false,
      "rack.run_once" => false,
      "rack.url_scheme" => "http",
      "rack.version" => "1.3",
      "rack.logger" => nil,
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
    uri = URI('http://localhost:6470/makeme')
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
    uri = URI('http://localhost:6470/makeme')
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

  def wild_one_test
    uri = URI('http://localhost:6470/wild/abc/one')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal('one - /wild/abc/one', res.body)
  end

  def wild_all_test
    uri = URI('http://localhost:6470/wild/all/x/y')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal('all - /wild/all/x/y', res.body)
  end

end
