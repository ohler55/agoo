
require 'agoo'

module Rack
  module Handler

    # The Rack::Handler::Agoo module is a handler for common rack config.rb files.
    module Agoo

      # Run the server. Options are the same as for Agoo::Server plus a :prot
      # and :root option.
      def self.run(handler, options={})
	port = 9292
	root = '.'
	default_handler = nil
	not_found_handler = nil
	path_map = {}

	default_handler = handler unless handler.nil?
	options.each { |k,v|
	  if :port == k
	    port = v.to_i
	    options.delete(k)
	  elsif :root == k
	    root = v
	    options.delete(k)
	  elsif k.is_a?(String) && k.start_with?('/')
	    path_map[k] = v
	    options.delete(k)
	  elsif k.nil?
	    not_found_handler = v
	    options.delete(k)
	  end
	}
	options[:thread_count] = 0
	server = ::Agoo::Server.new(port, root, options)
	path_map.each { |path,handler|
	  server.handle(nil, path, handler)
	}
	unless default_handler.nil?
	  server.handle(nil, '**', default_handler)
	end
	server.handle_not_found(not_found_handler) unless not_found_handler.nil?
	server.start
      end
      
    end
  end
end
