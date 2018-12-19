#!/usr/bin/env ruby

$: << File.dirname(__FILE__)
$root_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
%w(lib ext).each do |dir|
  $: << File.join($root_dir, dir)
end

require 'minitest'
require 'minitest/autorun'
require 'net/http'

require 'agoo'

class Query
  def initialize()
  end
end

class Schema
  attr_reader :query
  attr_reader :mutation
  attr_reader :subscription
 
  def initialize()
    @query = Query.new
  end
end

class GraphQLTest < Minitest::Test

  def test_graphql
    begin
      Agoo::Log.configure(dir: '',
			  console: true,
			  classic: true,
			  colorize: true,
			  states: {
			    INFO: false,
			    DEBUG: false,
			    connect: false,
			    request: false,
			    response: false,
			    eval: true,
			  })

      Agoo::Server.init(6472, 'root', thread_count: 1, graphql: '/graphql')
      Agoo::Server.start()

      load_test
      get_schema_test
    ensure
      Agoo::shutdown
    end
  end

  def load_test
    Agoo::GraphQL.schema(Schema.new) {
      Agoo::GraphQL.load(%|
type User {
  name: String!
  age: Int
}
|)
    }
    expect = %^type schema @ruby(class: "Schema") {
  query: Query
  mutation: Mutation
  subscription: Subscription
}

type Mutation {
}

type Query @ruby(class: "Query") {
}

type Subscription {
}

type User {
  name: String!
  age: Int
}

directive @ruby(class: String!) on SCHEMA | OBJECT
^
    content = Agoo::GraphQL.sdl_dump(with_descriptions: false, all: false)
    assert_equal(expect, content)
  end

  def get_schema_test
    uri = URI('http://localhost:6472/graphql/schema?with_desc=false&all=false')
    expect = %^type schema @ruby(class: "Schema") {
  query: Query
  mutation: Mutation
  subscription: Subscription
}

type Mutation {
}

type Query @ruby(class: "Query") {
}

type Subscription {
}

type User {
  name: String!
  age: Int
}

directive @ruby(class: String!) on SCHEMA | OBJECT
^
    req = Net::HTTP::Get.new(uri)
    req['Accept-Encoding'] = '*'
    req['User-Agent'] = 'Ruby'
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal('text/plain', res['Content-Type'])
    assert_equal(expect, content)
  end
end

