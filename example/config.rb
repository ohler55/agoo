
require 'agoo'

class FlyHandler
  def call(req)
    [ 200, { }, [ "flying fish" ] ]
  end
end

# A minimal startup of the Agoo rack handle includes a handler.
#Rack::Handler::Agoo.run(FlyHandler.new)

# A more complete example includes options for the port, root directory for
# static assets, handler to path mapping, and logging options.
Rack::Handler::Agoo.run(FlyHandler.new,
			port: 9292,
			root: 'root',
			"/fly/*" => FlyHandler.new,
			pedantic: false,
			log_dir: '',
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
# To run this example type the following then go to a browser and enter a URL of localhost:9292.
# ruby config.rb
