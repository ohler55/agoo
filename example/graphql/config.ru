# frozen_string_literal: true

# The Rack handler.
class FlyHandler
  def self.call(req)
    [ 200, { }, [ "flying fish" ] ]
  end
end

# Turn on INFO logging. The other option are just for documentation.
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

# The GraphQL classes and setup.
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
# allow for loading any static assets. It does set up the GraphQL on /graphql.

# $ bundle exec rackup -r agoo -s agoo -Ographql=/graphql

# Make requests on port 9292 to received responses. Try http://localhost:9292
# and http://localhost:9292/graphql?query={hello} which is the same as
# http://localhost:9292/graphql?query=%7Bhello%7D.
