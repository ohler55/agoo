
require 'agoo'

# Grand parent for the Agoo rack handler.
module Rack

  # Parent for the Agoo rack handler.
  module Handler

    # The Rack::Handler::Agoo module is a handler for common rack config.rb files.
    module Agoo

      # Run the server. Options are the same as for Agoo::Server plus a :port,
      # :root, :rmux, and :wc option.
      #
      #   - *:port [_Integer_] port to listen on
      #   - *:root [_String_] root or public directory for static assets
      #   - *:rmux [_true_|_false_] if true look in the root directory first before calling Ruby hooks
      #   - *:wc* [_Integer_] number of workers to fork. Defaults to one which is not to fork.
      #   - */path=MyHandler* path and class name to handle requests on that path
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
	  elsif :rmux == k
            options[:root_first] = false
	  elsif k.nil?
	    not_found_handler = v
	    options.delete(k)
	  elsif
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
	path_map.each { |path,handler|
			::Agoo::Server.handle(nil, path, handler)
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
