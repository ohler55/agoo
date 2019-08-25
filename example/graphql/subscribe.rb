# frozen_string_literal: true

require 'agoo'

Agoo::Log.configure(dir: '',
                    console: true,
                    classic: true,
                    colorize: true,
                    states: {
                      INFO: true,
                      DEBUG: true,
                      connect: true,
                      request: true,
                      response: true,
                      eval: true,
                      push: true
                    })

worker_count = 4
Agoo::Server.init(6464, '.', thread_count: 1, graphql: '/graphql')

# Empty response.
class Empty
  def self.call(_req)
    Agoo::GraphQL.publish('watch.me', Result.new('the time is', Time.now.to_s))
    [200, {}, []]
  end

  def static?
    true
  end
end

class Result
  attr_reader :word
  attr_reader :previous

  def initialize(word, previous)
    @word = word
    @previous = previous
  end
end

class Query
  def hello
    'Hello'
  end
end

class Mutation
  def initialize
    @previous = 'Hello'
  end

  def repeat(args={})
    word = args['word']
    Agoo::GraphQL.publish('watch.me', Result.new(word, @previous))
    @previous = word
  end
end

class Subscription
  def watch(args={})
    'watch.me'
  end
end

class Schema
  attr_reader :query
  attr_reader :mutation
  attr_reader :subscription

  def initialize
    @query = Query.new
    @mutation = Mutation.new
    @subscription = Subscription.new
  end
end

Agoo::Server.handle(:GET, '/', Empty)

Agoo::Server.start
Agoo::GraphQL.schema(Schema.new) {
  Agoo::GraphQL.load(%^
type Query {
  hello: String
}
type Mutation {
  repeat(word: String): String
}
type Subscription {
  watch: Result
}
type Result @ruby(class: "Result") {
  word: String
  previous: String
}
^)
}

sleep

# To run this example type the following
#
# ruby subscribe.rb
#
# Next go to a browser and enter a URL of localhost:6464/websocket.html. This
# will initiate a Websocket connection that is a GraphQL subscription.
#
# Next a trigger (mutation) is sent using curl that will cause a
# Agoo::GraphQL.publish. The result will be displayed on the browser page.
#
# curl -w "\n" -H "Content-Type: application/graphql" -d 'mutation{repeat(word: "A new word.")}' localhost:6464/graphql
