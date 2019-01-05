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

class Artist
  attr_reader :name
  attr_reader :songs
  attr_reader :origin
  
  def initialize(name, origin)
    @name = name
    @songs = []
    @origin = origin
  end

  def song(args={})
    @songs[args['name']]
  end

  # Only used by the Song to add itself to the artist.
  def add_song(song)
    @songs << song
  end
end

class Song
  attr_reader :name     # string
  attr_reader :artist   # reference
  attr_reader :duration # integer
  attr_reader :release  # time
  
  def initialize(name, artist, duration, release)
    @name = name
    @artist = artist
    @duration = duration
    @release = release
    artist.add_song(self)
  end
end

$songs_sdl = %^
type Query @ruby(class: "Query") {
  artist(name: String!): Artist
}

type Artist {
  name: String!
  songs: [Song]
  origin: [String]
}

type Song {
  name: String!
  artist: Artist
}
^

class Query
  attr_reader :artists

  def initialize(artists)
    @artists = artists
  end

  def artist(args={})
    @artists[args['name']]
  end
end

class Schema
  attr_reader :query
  attr_reader :mutation
  attr_reader :subscription
 
  def initialize()
    # Set up some data for testing.
    artist = Artist.new('Fazerdaze', ['Morningside', 'Auckland', 'New Zealand'])
    Song.new('Jennifer', artist, 240, Time.new(2017, 5, 5))
    Song.new('Lucky Girl', artist, 170, Time.new(2017, 5, 5))
    Song.new('Friends', artist, 194, Time.new(2017, 5, 5))
    Song.new('Reel', artist, 193, Time.new(2015, 11, 2))
    @artists = {artist.name => artist}

    @query = Query.new(@artists)
  end
end

class GraphQLTest < Minitest::Test

  SCHEMA_EXPECT = %^type schema @ruby(class: "Schema") {
  query: Query
  mutation: Mutation
  subscription: Subscription
}

type Artist {
  name: String!
  songs: [Song]
  origin: [String]
}

type Mutation {
}

type Query @ruby(class: "Query") {
  artist(name: String!): Artist
}

type Song {
  name: String!
  artist: Artist
}

type Subscription {
}

directive @ruby(class: String!) on SCHEMA | OBJECT
^

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
      get_query_test
      variable_query_test
      json_vars_query_test
      alias_query_test
      list_query_test
      array_query_test # returns an array

      # TBD list
      # TBD introspection
      # TBD POST
      #     fragment
      #     inline fragment
      #       on Song
      #       @skip
      #       @include

    ensure
      Agoo::shutdown
    end
  end

  def load_test
    Agoo::GraphQL.schema(Schema.new) {
      Agoo::GraphQL.load($songs_sdl)
    }
    content = Agoo::GraphQL.sdl_dump(with_descriptions: false, all: false)
    content.force_encoding('UTF-8')
    assert_equal(SCHEMA_EXPECT, content)
  end

  def get_schema_test
    uri = URI('http://localhost:6472/graphql/schema?with_desc=false&all=false')
    req = Net::HTTP::Get.new(uri)
    req['Accept-Encoding'] = '*'
    req['User-Agent'] = 'Ruby'
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal('application/graphql', res['Content-Type'])
    assert_equal(SCHEMA_EXPECT, content)
  end

  def get_query_test
    uri = URI('http://localhost:6472/graphql?query={artist(name:"Fazerdaze"){name}}&indent=2')
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^
    req = Net::HTTP::Get.new(uri)
    req['Accept-Encoding'] = '*'
    req['User-Agent'] = 'Ruby'
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal('application/json', res['Content-Type'])
    assert_equal(expect, content)
  end
  
  def variable_query_test
    uri = URI('http://localhost:6472/graphql?query={artist(name:"Fazerdaze"){name}}&indent=2')
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^
    req_test(uri, expect)
  end
  
  def alias_query_test
    uri = URI('http://localhost:6472/graphql?query=query withVar($name:String="Fazerdaze"){artist(name:$name){name}}&indent=2')
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^
    req_test(uri, expect)
  end
  
  def json_vars_query_test
    uri = URI('http://localhost:6472/graphql?query={artist(name:$name){name}}&indent=2&variables={"name":"Fazerdaze"}')
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^
    req_test(uri, expect)
  end
  
  def list_query_test
    uri = URI('http://localhost:6472/graphql?query=query withVar($name:String="Fazerdaze"){artist(name:$name){name,songs{name}}}&indent=2')
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze",
      "songs":[
        {
          "name":"Jennifer"
        },
        {
          "name":"Lucky Girl"
        },
        {
          "name":"Friends"
        },
        {
          "name":"Reel"
        }
      ]
    }
  }
}^
    req_test(uri, expect)
  end

  def array_query_test
    uri = URI('http://localhost:6472/graphql?query={artist(name:"Fazerdaze"){name,origin}}&indent=2')
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze",
      "origin":[
        "Morningside",
        "Auckland",
        "New Zealand"
      ]
    }
  }
}^
    req_test(uri, expect)
  end
  
  def req_test(uri, expect)
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal(expect, content)
  end
end
