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

    Agoo::Server.init(6474, 'root', thread_count: 1)
    Agoo::Server.domain('domain1.ohler.com', './domain1')
    Agoo::Server.domain(/^domain([0-9]+).ohler.com$/, './domain$(1)')
    Agoo::Server.start()

    @@server_started = true
  end

  def setup
    unless @@server_started
      start_server
    end
  end

  Minitest.after_run {
    Agoo::shutdown
  }

  def test_one
    uri = URI('http://localhost:6474/name.txt')
    req = Net::HTTP::Get.new(uri)
    req['Host'] = 'domain1.ohler.com'
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal('domain one
', content)
  end

  def test_two
    uri = URI('http://localhost:6474/name.txt')
    req = Net::HTTP::Get.new(uri)
    req['Host'] = 'domain2.ohler.com'
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal('domain two
', content)
  end

  def test_nothing
    uri = URI('http://localhost:6474/name.txt')
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    assert_equal("404", res.code)
  end

end
