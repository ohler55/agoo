
require 'agoo' unless defined?(Agoo)

# Grand parent for the Agoo rack handler.
module Rack

  # Parent for the Agoo rack handler.
  module Handler

    # The Rack::Handler::Agoo module is a handler for common rack config.rb files.
    module Agoo

      # Run the server. Options are the same as for Agoo::Server plus a :port,
      # :root, :rmux, and :wc option.
      def self.run(handler, options={})
	port = 0
	root = './public'
        root_set = false
	worker_count = 1;
	default_handler = nil
	not_found_handler = nil
	path_map = {}
	verbose = 1
	log_dir = nil
	classic = true
	console = true
	colorize = true
	binds = nil
	graphql = nil
        options[:root_first] = true # the default for rack

	default_handler = handler unless handler.nil?
	options.each { |k,v|
	  if :port == k || :p == k
	    port = v.to_i
	    options.delete(k)
	  elsif :root == k || :dir == k || :d == k
	    root = v
            root_set = true
	    options.delete(k)
	  elsif :wc == k || :workers == k
	    worker_count = v.to_i
	    options.delete(k)
	  elsif :rmux == k || :root_first == k || :f == k
            options[:root_first] = false
	  elsif k.nil?
	    not_found_handler = v
	    options.delete(k)
	  elsif :graphql == k || :g == k
	    graphql = v
	    options.delete(k)
	  elsif :s == k || :silent == k
	    verbose = 0
	    options.delete(k)
	  elsif :v == k || :verbose == k
	    verbose = 2
	    options.delete(k)
	  elsif :debug == k
	    verbose = 3
	    options.delete(k)
	  elsif :b == k || :bind == k
	    binds = v.split(',')
	    options.delete(k)
	  elsif :log_dir == k
	    log_dir = v
	    options.delete(k)
	  elsif :log_classic == k
	    classic = true
	    options.delete(k)
	  elsif :no_log_classic == k
	    classic = false
	    options.delete(k)
	  elsif :log_console == k
	    console = true
	    options.delete(k)
	  elsif :no_log_console == k
	    console = false
	    options.delete(k)
	  elsif :log_colorize == k
	    colorize = true
	    options.delete(k)
	  elsif :no_log_colorize == k
	    colorize = false
	    options.delete(k)
	  elsif :help == k || :h == k
	    puts %|
Agoo is a Ruby web server that supports Rack. The follwing options are available
using the -O NAME[=VALUE] option of rackup. Note that if binds are provided the
-p PORT option will be ignored but -Op=PORT can be used.

  -O h, help                 Show this display.
  -O s, silent               Silent.
  -O v, verbose              Verbose.
  -O debug                   Very verbose.
  -O f, rmux, root_first     Check the root directory before the handle paths.
  -O p, port=PORT            Port to listen on.
  -O b, bind=URL             URLs to receive connections on, comma separated.
                             Examples:
                               "http ://127.0.0.1:6464"
                               "unix:///tmp/agoo.socket"
                               "http ://[::1]:6464
                               "http ://:6464"
  -O d, dir, root=DIR        Directory to serve static assets from.
  -O g, graphql=PATH         URL path for GraphQL requests.
  -O t, threads=COUNT        Number of threads to use.
  -O w, workers=COUNT        Number of workers to use.
     -O log_dir=DIR          Log file directory.
     -O [no_]log_classic     Classic log entries instead of JSON.
     -O [no_]log_console     Display log entries on the console.
     -O [no_]log_colorize    Display log entries in color.
  -O /path=MyHandler path and class name to handle requests on that path

|
	    exit(true)
	  else
            k = k.to_s
            if k.start_with?('/')
              path_map[k] = v
              options.delete(k)
            end
	  end
	}
	options[:thread_count] = 0
	options[:worker_count] = worker_count
	if binds.nil?
	  options[:Port] = port unless port == 0
	else
	  options[:bind] = binds
	  options[:Port] = port
	end
	options[:graphql] = graphql unless graphql.nil?

	::Agoo::Log.configure(dir: log_dir,
			      console: console,
			      classic: classic,
			      colorize: colorize,
			      states: {
				INFO: 1 <= verbose,
				DEBUG: 3 <= verbose,
				connect: 2 <= verbose,
				request: 2 <= verbose,
				response: 2 <= verbose,
				eval: 2 <= verbose,
				push: 2 <= verbose,
			      })
	::Agoo::Server.init(port, root, options)
	path_map.each { |path,h|
	  ::Agoo::Server.handle(nil, path, h)
	}
        begin
          # If Rails is loaded this should work else just ignore.
	  if const_defined?(:Rails)
            ::Agoo::Server.path_group('/assets', ::Rails.configuration.assets.paths)
            root = Rails.public_path unless root_set
	  end
        rescue Exception
        end
	unless default_handler.nil?
	  ::Agoo::Server.handle(nil, '**', default_handler)
	end
	::Agoo::Server.handle_not_found(not_found_handler) unless not_found_handler.nil?
	::Agoo::Server.start
      end

      def self.shutdown
	::Agoo::shutdown
      end

    end
  end
end

begin
  ::Rack::Handler.register('agoo', 'Rack::Handler::Agoo')
  ::Rack::Handler.register('Agoo', 'Rack::Handler::Agoo')
rescue Exception
  # Rack or Rack::Handler.register has not been required.
end
