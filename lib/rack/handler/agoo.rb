
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
	port = 9292
	root = './public'
        root_set = false
	worker_count = 1;
	default_handler = nil
	not_found_handler = nil
	path_map = {}
        options[:root_first] = true # the default for rack

	default_handler = handler unless handler.nil?
	options.each { |k,v|
	  if :port == k || :p == k
	    port = v.to_i
	    options.delete(k)
	  elsif :root == k
	    root = v
            root_set = true
	    options.delete(k)
	  elsif :wc == k
	    worker_count = v.to_i
	    options.delete(k)
	  elsif :rmux == k || :root_first == k || :f == k
            options[:root_first] = false
	  elsif k.nil?
	    not_found_handler = v
	    options.delete(k)
	  elsif :graphql == k
	    # leave as is
	  elsif :bind == k
	    # TBD
	  elsif :log_dir == k
	    # TBD
	  elsif :log_classic == k
	    # TBD
	  elsif :no_log_classic == k
	    # TBD
	  elsif :log_console == k
	    # TBD
	  elsif :no_log_console == k
	    # TBD
	  elsif :log_colorize == k
	    # TBD
	  elsif :no_log_colorize == k
	    # TBD
	  elsif :help == k || :h == k
	    puts %|
Agoo is a Ruby web server that supports Rack. The follwing options are available
using the -O NAME[=VALUE] option of rackup.

  -O h, help                 Show this display.
  -O s, silent               Silent.
  -O v, verbose              Increase verbosity.
  -O f, rmux, root_first     Check the root directory before the handle paths.
  -O p, port=PORT            Port to listen on.
  -O b, bind=URL             URL to receive connections on. Examples:
                               "http ://127.0.0.1:6464"
                               "unix:///tmp/agoo.socket"
                               "http ://[::1]:6464
                               "http ://:6464"
  -O d, dir, root=DIR        Directory to serve static assets from.
  -O g, graphql=PATH         URL path for GraphQL requests.
  -O r, require=FILE         Ruby require.
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
	::Agoo::Server.init(port, root, options)
	path_map.each { |path,h|
	  ::Agoo::Server.handle(nil, path, h)
	}
        begin
          # If Rails is loaded this should work else just ignore.
          ::Agoo::Server.path_group('/assets', Rails.configuration.assets.paths)
          root = Rails.public_path unless root_set
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
