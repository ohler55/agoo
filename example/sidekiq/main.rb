
require 'agoo'
require 'sidekiq/web'

# Setting the thread count to 0 causes the server to use the current
# thread. Greater than zero runs the server in a separate thread and the
# current thread can be used for other tasks as long as it does not exit.
Agoo::Log.configure(dir: '',
		    console: true,
		    classic: true,
		    colorize: true,
		    states: {
		      INFO: true,
		      DEBUG: false,
		      connect: true,
		      request: true,
		      response: true,
		      eval: true,
		      push: false,
		    })

class Stat
  def self.call(req)
    stats = Sidekiq::Stats.new
    workers = Sidekiq::Workers.new
    content = %|
<p>Processed: #{stats.processed}</p>
<p>Enqueued: #{stats.enqueued}</p>
<p>In Progress: #{workers.size}</p>
<p><a href='/'>Refresh</a></p>
<p><a href='/add_job'>Add Job</a></p>
<p><a href='/sidekiq'>Dashboard</a></p>
    |
    [ 200, { }, [ content ] ]
  end
end

Agoo::Server.init(6464, 'root', thread_count: 0)

Sidekiq::Web.set :sessions, false
Sidekiq::Web.use Rack::Session::Cookie
Sidekiq::Web.use Rack::Auth::Basic do |username, password|
  username == 'hello' && password == 'world'
end
Sidekiq::Web.use Rack::Auth::Basic do |username, password|
  [username, password] == [Settings.sidekiq.username, Settings.sidekiq.password]
end

Agoo::Server.handle(:GET, "/sidekiq", Sidekiq::Web)
Agoo::Server.handle(:GET, "/sidekiq/**", Sidekiq::Web)
Agoo::Server.handle(:GET, "/", Stat)

Agoo::Server.start()

# To run this example type the following then go to a browser and enter a URL of localhost:6464/hello.
# ruby hello.rb
