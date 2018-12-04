
require 'agoo'

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
		      connect: false,
		      request: false,
		      response: false,
		      eval: false,
		      push: false,
		    })

Agoo::Server.init(6464, File.dirname(__FILE__), thread_count: 0)

class Uploader
  def self.call(req)
    multipart = req['rack.input'].read
    content = extract_file_content(multipart)

    puts "content: '#{content}'"
    
    [ 200, { }, [ "Uploaded" ] ]
  end

  # A multipart/form-data starts with something like
  #   ------WebKitFormBoundaryJ2Qq1MmR2b6XBeAm
  #   Content-Disposition: form-data; name="file"; filename="foo.rb"
  #   Content-Type: text/x-ruby-script
  #
  # and ends like this.
  #   Content-Disposition: form-data; name="submit"
  #
  #   Upload File
  #   ------WebKitFormBoundaryJ2Qq1MmR2b6XBeAm--
  #
  def self.extract_file_content(multipart)

    # First line is the boundary.
    i = multipart.index("\r\n")
    return 'broken' if i.nil?
    boundary = multipart[0..i]

    # find the start of the contents by looking for \r\n\r\n
    i = multipart.index("\r\n\r\n")
    return 'broken' if i.nil?
    tail = multipart[i+4..-1]

    i = tail.rindex(boundary)
    return 'broken' if i.nil?
    tail[0...i]
  end

end

# Register the handler before calling start.
Agoo::Server.handle(:POST, "/", Uploader)

Agoo::Server.start()

# To run this example type the following then go to a browser and enter a URL of localhost:6464.
# ruby upload.rb
