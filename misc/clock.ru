# frozen_string_literal: true

require "rack"
require 'agoo'

class TickTock
  # Only needed for the non-extended approach.
  attr_accessor: sender

  def initialize(env)
  end

  # I could not come up with any other nice alternatives. Lets go with
  # extended.
  def on_open(sender = nil)
    # 1) All that is needed for a subscribe is a channel, subject, or
    # filter. Filter is going to be very implemenation specific so I suggest
    # channel/subject where channel is a subset of a subject or rather a
    # subject without wildcards. I don't think there is a need for a
    # 
    # [Are you suggesting 'subject' as an alternative to 'pattern'? I think 'pattern' is clearer...]
    # [...or do you mean that 'subject' forces a server to test the string and decide?]
    # 
    # 2) Some servers might choose to implement optional extended features.
    # For example, some servers might support WebSocket encoding semantics
    # (i.e., text vs. binary op-codes, data compression / encoding) or even
    # SSE variations (i.e, Base64 encoding for binary data support).
    # 
    # To allow for these further extensions, I believe a Hash (or "named arguments")
    # scheme is probably preferable.
    subscribe channel: :time
    # It allows for something like:
    subscribe channel: :time, as: :text
    # vs
    ## Removed(?):
    # subscribe('time')
  end

  def on_close
    puts "closing"
  end

  # If an SSE connection this would never be called.
  def on_message(data)
    puts "received #{data}"
    write("echo: #{data}")
  end

  # If the non-extended approach is used then this would provide the same
  # functionality as the entended.
  def write(data)
    return super if defined?(super)
    # non-extended only
    @sender.write(data)
  end

end

class FlyHandler

  def call(env)
    # 2) These are not the best terms. Lets figure out something reasonable
    # that is not tied to WebSockets but allows SSE or WebSockets and use a
    # env[`upgrade.type'] if the developer cares which type of connection they
    # are getting.
    if env['upgrade.push?'.freeze]
      env['upgrade.push'.freeze] = TickTock.new(env)
      # This threw me off before and it's why I asked about how middleare was
      # going to deny a connection. I think this needs to be a valid HTTP
      # status code in all case. Maybe 101?
      return [101, {}, []]
    end
    [ 200, { }, [ "flying fish clock" ] ]
  end

end

run FlyHandler.new

loop do
  now = Time.now

  # 3) Similar to subscribe. I prefer a just 2 arguments, a subject/channel
  # and the data to publish.

  # The Redis approach.
  # 
  # I have a preference for this one just because that's what I did in the past,
  # (I used 'channel' and 'pattern' as the two names)...
  # ...but we can surely use the 'subject' name if you feel it's better. I just
  # think it might be a bit confusing where pattern matching is concerned.
  Rack.publish(channel: :time,
	       message: "%02d:%02d:%02d" % [now.hour, now.min, now.sec])
  # For publishing channel and subject would appear the same. Maybe even alias
  # channel and subject.
  # 
  # I prefer deciding on a name rather than aliasing. Creating an alias is a
  # good tool for backwards compatibility, but isn't super clear for a new spec.
  # 
  # The only naming preference I ask for is that it is consistent with naming
  # for 'subscribe'	
  Rack.publish(subject: 'time',
               message: "%02d:%02d:%02d" % [now.hour, now.min, now.sec])
	
  # Or publish with two args. First argument could be a channel ID or a
  # subject. Publish is always done without wild cards so my.cool.time would
  # be a valid subject or channel ID.
  # 
  # I can't think of any possible extensions to 'publish', but I think it would be
  # better to leave it as a possibility.
  # 
  # Besides, I think it's better to use a Hash so the API is similar to 'subscribe'.
  ## Removed:
  # Rack.publish('time', "%02d:%02d:%02d" % [now.hour, now.min, now.sec])
  
  sleep(1)
end


# A minimal startup of the Agoo rack handle using rackup and expecting a
# WebSockets or SSE connection. Note this does not allow for loading any
# static assets.
# $ bundle exec rackup

# Make requests on port 9292 to received responses.
