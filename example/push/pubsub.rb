
require 'agoo'

class Clock
  def self.call(env)
    unless env['rack.upgrade?'].nil?
      env['rack.upgrade'] = Clock
      [ 200, { }, [ ] ]
    else
      [ 404, { }, [ ] ]
    end
  end

  def self.on_open(client)
    client.subscribe('time')
  end

  Thread.new {
    loop do
      now = Time.now
      Agoo.publish('time', "%02d:%02d:%02d" % [now.hour, now.min, now.sec])
      sleep(1)
    end
  }
end

Agoo::Server.init(6464, '.', thread_count: 0)
Agoo::Server.handle(:GET, '/upgrade', Clock)
Agoo::Server.start()

# Quick start:
#
# ruby pubsub.rb
#
# Open either http://localhost:6464/websocket.html or
# http://localhost:6464/sse.html.
