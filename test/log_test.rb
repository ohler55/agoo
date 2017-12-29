#!/usr/bin/env ruby

$: << File.dirname(__FILE__)
$root_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
%w(lib ext).each do |dir|
  $: << File.join($root_dir, dir)
end

require 'minitest'
require 'minitest/autorun'

require 'agoo'

class LogStateTest < Minitest::Test

  def test_log_state
    begin
      server = Agoo::Server.new(6464, '.')

      error_state_test(server)
      warn_state_test(server)
      info_state_test(server)
      debug_state_test(server)
      request_state_test(server)
      response_state_test(server)
      eval_state_test(server)
    ensure
      server.shutdown
    end
  end
  
  def error_state_test(server)
    assert(server.error?)
    server.set_log_state('ERROR', false)
    refute(server.error?)
    server.set_log_state('ERROR', true)
  end

  def warn_state_test(server)
    assert(server.warn?)
    server.set_log_state('WARN', false)
    refute(server.warn?)
    server.set_log_state('WARN', true)
  end

  def info_state_test(server)
    refute(server.info?)
    server.set_log_state('INFO', true)
    assert(server.info?)
  end

  def debug_state_test(server)
    refute(server.debug?)
    server.set_log_state('DEBUG', true)
    assert(server.debug?)
    server.set_log_state('DEBUG', false)
  end

  def request_state_test(server)
    refute(server.log_state('request'))
    server.set_log_state('request', true)
    assert(server.log_state('request'))
    server.set_log_state('request', false)
  end

  def response_state_test(server)
    refute(server.log_state('response'))
    server.set_log_state('response', true)
    assert(server.log_state('response'))
    server.set_log_state('response', false)
  end

  def eval_state_test(server)
    refute(server.log_state('eval'))
    server.set_log_state('eval', true)
    assert(server.log_state('eval'))
    server.set_log_state('eval', false)
  end

  def unknown_state_test(server)
    assert_raises(ArgumentError) {
      server.log_state('unknown')
    }
    assert_raises(ArgumentError) {
      server.set_log_state('unknown', true)
    }
  end

end

class LogClassicTest < Minitest::Test

  def test_log_classic
    `rm -rf log`
    begin
      server = Agoo::Server.new(6464, '.',
				log_dir: 'log',
				log_console: false,
				log_classic: true,
				log_colorize: false,
				log_states: {
				  INFO: true,
				  DEBUG: true,
				  eval: true,
				})
      error_test(server)
      warn_test(server)
      info_test(server)
      debug_test(server)
      log_eval_test(server)
    ensure
      server.shutdown
    end
  end
  
  def error_test(server)
    server.error('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log')
    assert_match(/ERROR: my message/, content)
  end

  def warn_test(server)
    server.warn('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log')
    assert_match(/WARN: my message/, content)
  end

  def info_test(server)
    server.info('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log')
    assert_match(/INFO: my message/, content)
  end

  def debug_test(server)
    server.debug('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log')
    assert_match(/DEBUG: my message/, content)
  end

  def log_eval_test(server)
    server.log_eval('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log')
    assert_match(/eval: my message/, content)
  end

end

class LogJsonTest < Minitest::Test

  def test_log_json
    `rm -rf log`
    begin
      server = Agoo::Server.new(6464, '.',
				log_dir: 'log',
				log_max_files: 2,
				log_max_size: 10,
				log_console: false,
				log_classic: false,
				log_colorize: false,
				log_states: {
				  INFO: true,
				  DEBUG: true,
				  eval: true,
				})
      error_test(server)
      warn_test(server)
      info_test(server)
      debug_test(server)
      eval_test(server)
    ensure
      server.shutdown
    end
  end
  
  def error_test(server)
    server.error('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log.1')
    assert_match(/"where":"ERROR"/, content)
    assert_match(/"level":1/, content)
    assert_match(/"what":"my message"/, content)
  end

  def warn_test(server)
    server.warn('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log.1')
    assert_match(/"where":"WARN"/, content)
    assert_match(/"level":2/, content)
    assert_match(/"what":"my message"/, content)
  end

  def info_test(server)
    server.info('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log.1')
    assert_match(/"where":"INFO"/, content)
    assert_match(/"level":3/, content)
    assert_match(/"what":"my message"/, content)
  end

  def debug_test(server)
    server.debug('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log.1')
    assert_match(/"where":"DEBUG"/, content)
    assert_match(/"level":4/, content)
    assert_match(/"what":"my message"/, content)
  end

  def eval_test(server)
    server.log_eval('my message')
    server.log_flush(1.0)
    content = IO.read('log/agoo.log.1')
    assert_match(/"where":"eval"/, content)
    assert_match(/"level":3/, content)
    assert_match(/"what":"my message"/, content)
  end

end

class LogRollTest < Minitest::Test

  def setup
    `rm -rf log`
    begin
      server = Agoo::Server.new(6464, '.',
				 log_dir: 'log',
				 log_max_files: 2,
				 log_max_size: 150,
				 log_console: false,
				 log_classic: true,
				 log_colorize: false,
				 log_states: {
				   INFO: true,
				   DEBUG: true,
				   eval: true,
				 })
      10.times { |i|
	@server.info("my #{i} message")
      }
      @server.log_flush(1.0)
      content = IO.read('log/agoo.log')
      assert_match(/INFO: my 9 message/, content)

      content = IO.read('log/agoo.log.1')
      assert_match(/INFO: my 6 message/, content)
      assert_match(/INFO: my 7 message/, content)
      assert_match(/INFO: my 8 message/, content)

      content = IO.read('log/agoo.log.2')
      assert_match(/INFO: my 3 message/, content)
      assert_match(/INFO: my 4 message/, content)
      assert_match(/INFO: my 5 message/, content)

      assert_raises() {
	IO.read('log/agoo.log.3')
      }
    ensure
      @server.shutdown
    end
  end

end
