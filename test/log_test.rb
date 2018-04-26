#!/usr/bin/env ruby

$: << File.dirname(__FILE__)
$root_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
%w(lib ext).each do |dir|
  $: << File.join($root_dir, dir)
end

require 'minitest'
require 'minitest/autorun'

require 'agoo'

class LogTest < Minitest::Test

  def test_log
    `rm -rf log`
    begin
      Agoo::Log.configure(dir: 'log',
			     max_files: 2,
			     max_size: 150,
			     console: false,
			     classic: true,
			     colorize: false,
			     states: {
			       INFO: true,
			       DEBUG: true,
			       eval: true,
			     })

      10.times { |i|
	Agoo::Log.info("my #{i} message")
      }
      Agoo::Log.flush(1.0)
      sleep(0.05)
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

      error_state_test()
      warn_state_test()
      info_state_test()
      debug_state_test()
      request_state_test()

      Agoo::Log.max_size = 10000

      Agoo::Log.classic
      Agoo::Log.set_state('DEBUG', true)
      Agoo::Log.set_state('eval', true)
      classic_error_test()
      classic_warn_test()
      classic_info_test()
      classic_debug_test()
      classic_log_test()

      Agoo::Log.json
      json_error_test()
      json_warn_test()
      json_info_test()
      json_debug_test()
      json_log_test()

    ensure
      Agoo::Log.shutdown
    end
  end

  def error_state_test()
    assert(Agoo::Log.error?)
    Agoo::Log.set_state('ERROR', false)
    refute(Agoo::Log.error?)
    Agoo::Log.set_state('ERROR', true)
  end

  def warn_state_test()
    assert(Agoo::Log.warn?)
    Agoo::Log.set_state('WARN', false)
    refute(Agoo::Log.warn?)
    Agoo::Log.set_state('WARN', true)
  end

  def info_state_test()
    assert(Agoo::Log.info?)
    Agoo::Log.set_state('INFO', false)
    refute(Agoo::Log.info?)
    Agoo::Log.set_state('INFO', true)
  end

  def debug_state_test()
    Agoo::Log.set_state('DEBUG', false)
    refute(Agoo::Log.debug?)
    Agoo::Log.set_state('DEBUG', true)
    assert(Agoo::Log.debug?)
    Agoo::Log.set_state('DEBUG', false)
  end

  def request_state_test()
    Agoo::Log.set_state('request', false)
    refute(Agoo::Log.state('request'))
    Agoo::Log.set_state('request', true)
    assert(Agoo::Log.state('request'))
    Agoo::Log.set_state('request', false)
  end

  def unknown_state_test()
    assert_raises(ArgumentError) {
      Agoo::Log.state('unknown')
    }
    assert_raises(ArgumentError) {
      Agoo::Log.set_state('unknown', true)
    }
  end

  def classic_error_test()
    Agoo::Log.rotate
    Agoo::Log.error('my message')
    Agoo::Log.flush(1.0)
    sleep(0.05) # seems to be needed to assure writes actually happened even though flush was called.
    content = IO.read('log/agoo.log')
    assert_match(/ERROR: my message/, content)
  end

  def classic_warn_test()
    Agoo::Log.rotate
    Agoo::Log.warn('my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/WARN: my message/, content)
  end

  def classic_info_test()
    Agoo::Log.rotate
    Agoo::Log.info('my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/INFO: my message/, content)
  end

  def classic_debug_test()
    Agoo::Log.rotate
    Agoo::Log.debug('my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/DEBUG: my message/, content)
  end

  def classic_log_test()
    Agoo::Log.rotate
    Agoo::Log.log('eval', 'my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/eval: my message/, content)
  end

  def json_error_test()
    Agoo::Log.rotate
    Agoo::Log.error('my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/"where":"ERROR"/, content)
    assert_match(/"level":1/, content)
    assert_match(/"what":"my message"/, content)
  end

  def json_warn_test()
    Agoo::Log.rotate
    Agoo::Log.warn('my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/"where":"WARN"/, content)
    assert_match(/"level":2/, content)
    assert_match(/"what":"my message"/, content)
  end

  def json_info_test()
    Agoo::Log.rotate
    Agoo::Log.info('my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/"where":"INFO"/, content)
    assert_match(/"level":3/, content)
    assert_match(/"what":"my message"/, content)
  end

  def json_debug_test()
    Agoo::Log.rotate
    Agoo::Log.debug('my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/"where":"DEBUG"/, content)
    assert_match(/"level":4/, content)
    assert_match(/"what":"my message"/, content)
  end

  def json_log_test()
    Agoo::Log.rotate
    Agoo::Log.log('eval', 'my message')
    Agoo::Log.flush(1.0)
    sleep(0.05)
    content = IO.read('log/agoo.log')
    assert_match(/"where":"eval"/, content)
    assert_match(/"level":3/, content)
    assert_match(/"what":"my message"/, content)
  end

end
