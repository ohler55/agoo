# frozen_string_literal: true

require "rack"
require 'agoo'

class TickTock
  # Only needed for the non-extended approach.
  attr_accessor: sender

  def initialize(env)
  end

  # 1) Discuss using extend or passin in a sender object.
  def on_open(sender = nil)
    # for iodine compatibility, as iodine doen't provide a sender object
    @sender = sender ? sender : self
    # 2) subscriber options. If extended then this works.
    subscribe channel: :time
    # using the same extended approach this would also work. Argument could be
    # a channel or a subject.
    subscribe('time')
    # If not using the extended approach then something like this. Of course
    # 'sender' is not the right name if the object is both a sender and a
    # handle to the server.
    sender.subscribe(self, :time)

    # PCO - I don't like using extend as it modifies an object owned by
    # another package. It comes across as magic. I can see how extend makes
    # the API cleaner though. I'd like to explore other options if there are
    # any. Just some more thought.
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
    # 3) These are not the best terms. Lets figure out something reasonable
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
  # The Redis approach.
  Rack.publish(channel: :time,
	       message: "%02d:%02d:%02d" % [now.hour, now.min, now.sec])
  # For publishing channel and subject would appear the same. Maybe even alias
  # channel and subject.
  Rack.publish(subject: 'time',
	       message: "%02d:%02d:%02d" % [now.hour, now.min, now.sec])
  # Or publish with two args. First argument could be a channel ID or a
  # subject. Publish is always done without wild cards so my.cool.time would
  # be a valid subject or channel ID.
  Rack.publish('time', "%02d:%02d:%02d" % [now.hour, now.min, now.sec])
  
  sleep(1)
end


# A minimal startup of the Agoo rack handle using rackup and expecting a
# WebSockets or SSE connection. Note this does not allow for loading any
# static assets.
# $ bundle exec rackup

# Make requests on port 9292 to received responses.
