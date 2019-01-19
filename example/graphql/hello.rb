require 'agoo'

class Query
  def hello
    'Agoo GraphQL says Hello'
  end
end

class Schema
  attr_reader :query

  def initialize
    @query = Query.new()
  end
end

puts %^\nopen 'localhost:6464/graphql?query={hello}' in a browser.\n\n^

Agoo::Server.init(6464, 'root', thread_count: 1, graphql: '/graphql')
Agoo::Server.start()
Agoo::GraphQL.schema(Schema.new) {
  Agoo::GraphQL.load(%^type Query { hello: String }^)
}
sleep

# ruby hello.rb
