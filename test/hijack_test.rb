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
  class HijackHandler
    def call(env)
      io = env['rack.hijack'].call
      io.write(%|HTTP/1.1 200 OK
Content-Length: 11

hello world|)
      io.flush
      [ -1, {}, []]
    end
  end

  Minitest.after_run {
    GC.start
    Agoo::shutdown
  }

  def test_hijack
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

    Agoo::Server.init(6471, 'root', thread_count: 1)

    handler = HijackHandler.new
    Agoo::Server.handle(:GET, "/hijack", handler)

    Agoo::Server.start()

    jack
  end

  def jack
    uri = URI('http://localhost:6471/hijack')
    req = Net::HTTP::Get.new(uri)
    # Set the headers the way we want them.
    req['Accept-Encoding'] = '*'
    req['User-Agent'] = 'Ruby'
    req['Host'] = 'localhost:6471'

    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body

    assert_equal('hello world', content, 'content mismatch')
  end

end
