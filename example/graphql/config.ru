# frozen_string_literal: true

class FlyHandler
  def self.call(req)
    [ 200, { }, [ "flying fish" ] ]
  end
end

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
                      push: false
                    })

worker_count = 1
worker_count = ENV['AGOO_WORKER_COUNT'].to_i if ENV.key?('AGOO_WORKER_COUNT')
Agoo::Server.init(9292, '.', thread_count: 1, worker_count: worker_count, graphql: '/graphql')

class Query
  def hello
    'Hello'
  end
end

class Mutation
  def initialize
    @last = 'Hello'
  end

  def repeat(args={})
    @last = args['word']
  end
end

class Schema
  attr_reader :query
  attr_reader :mutation

  def initialize
    @query = Query.new()
    @mutation = Mutation.new()
  end
end

Agoo::GraphQL.schema(Schema.new) {
  Agoo::GraphQL.load(%^
type Query {
  hello: String
}
type Mutation {
  repeat(word: String): String
}
^)
}

run FlyHandler


# A minimal startup of the Agoo rack handle using rackup. Note this does not
# allow for loading any static assets.
# $ bundle exec rackup -r agoo -s agoo

# Make requests on port 9292 to received responses.

# TBD how to set options when using rack? maybe have to use command line
# TBD
