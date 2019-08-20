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
    Agoo.publish('watch.me', '{"watch":"hello"}')
    [200, {}, []]
  end

  def static?
    true
  end
end

class Query
  def hello
    Agoo::GraphQL.publish('watch.me', "Whats the time?")
    'Hello'
  end
end

class Mutation
  def initialize
    @last = 'Hello'
  end

  def repeat(args={})
    word = args['word']
    # TBD graphql_publish('word', "#{@last} => #{word}")
    @last = word
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
type Result {
  word: String
}
^)
}

sleep
