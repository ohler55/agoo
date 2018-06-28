
require 'oj'
require 'agoo'

Agoo::Log.configure(dir: '',
		    console: true,
		    classic: true,
		    colorize: true,
		    states: {
		      INFO: true,
		      DEBUG: false,
		      connect: false,
		      request: false,
		      response: false,
		      eval: false,
		      push: false,
		    })

Agoo::Server.init(6464, '.', thread_count: 0)

# Keep it simple and just use the class since the use is stateless.
class Listen
  def self.call(env)
    unless env['rack.upgrade?'].nil?
      env['rack.upgrade'] = self
      [ 200, { }, [ ] ]
    else
      [ 404, { }, [ ] ]
    end
  end

  def self.on_open(client)
    client.subscribe('shootout')
  end

  def self.on_close(client)
    client.unsubscribe('shootout')
  end

  def self.on_message(client, data)
    cmd, payload = Oj.load(data).values_at('type', 'payload')
    if cmd == 'echo'
      client.write(data)
    else
      # data = {type: 'broadcast', payload: payload}.to_json
      Agoo.publish('shootout', Oj.dump({type: "broadcast", payload: payload}, mode: :strict))
      client.write(Oj.dump({type: "broadcastResult", payload: payload}, mode: :strict))
    end
  end
end

Agoo::Server.handle(:GET, "/upgrade", Listen)
Agoo::Server.start()

# To run this with the shootout benchmarking tool:
# ruby shootout.rb
