
require 'agoo'

# Grand parent for the Agoo rack handler.
module Rack

  # Parent for the Agoo rack handler.
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
	  if :port == k || :p == k
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
	::Agoo::Server.init(port, root, options)
	path_map.each { |path,handler|
			::Agoo::Server.handle(nil, path, handler)
	}
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
