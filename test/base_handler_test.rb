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
  @@server_started = false

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
      elsif 'DELETE' == req.request_method
	res.code = 200
	res.body = req.body


      end
    end
  end

  class WildHandler
    def initialize(name)
      @name = name
    end

    def on_request(req, res)
      res.body = "#{@name} - #{req.path_info}"
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

    Agoo::Server.init(6470, 'root', thread_count: 1)

    handler = TellMeHandler.new

    Agoo::Server.handle(:GET, "/tellme", handler)
    Agoo::Server.handle(:POST, "/makeme", handler)
    Agoo::Server.handle(:PUT, "/makeme", handler)
    Agoo::Server.handle(:GET, "/wild/*/one", WildHandler.new('one'))
    Agoo::Server.handle(:GET, "/wild/all/**", WildHandler.new('all'))
    Agoo::Server.handle(:DELETE, "/notme", handler)

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

  def test_eval
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
      "HTTP_ACCEPT" => "application/json",
      "HTTP_ACCEPT_ENCODING" => "*",
      "HTTP_USER_AGENT" => "Ruby",
      "PATH_INFO" => "/tellme",
      "QUERY_STRING" => "a=1",
      "REQUEST_METHOD" => "GET",
      "SCRIPT_NAME" => "",
      "SERVER_NAME" => "localhost",
      "SERVER_PORT" => "6470",
      "rack.errors" => nil,
      "rack.input" => nil,
      "rack.multiprocess" => false,
      "rack.multithread" => false,
      "rack.run_once" => false,
      "rack.url_scheme" => "http",
      "rack.version" => [1, 3],
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

  def test_post
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

  def test_put
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

  def test_delete
    uri = URI('http://localhost:6470/notme')
    req = Net::HTTP::Delete.new(uri)
    # Set the headers the way we want them.
    req['Accept-Encoding'] = '*'
    req['Accept'] = 'application/json'
    req['User-Agent'] = 'Ruby'
    req.body = 'goodbye'

    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal('goodbye', res.body)
  end

  def test_wild_one
    uri = URI('http://localhost:6470/wild/abc/one')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal('one - /wild/abc/one', res.body)
  end

  def test_wild_all
    uri = URI('http://localhost:6470/wild/all/x/y')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal('all - /wild/all/x/y', res.body)
  end

end
