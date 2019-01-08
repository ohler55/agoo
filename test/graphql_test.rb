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

  def genre_songs(args={})
    g = args['genre']
    a = []
    @songs.each { |s|
      a << s if g == s.genre
    }
    a
  end

  def songs_after(args={})
    t = args['time']
    a = []
    @songs.each { |s|
      a << s if t <= s.release
    }
    a
  end
end

class Song
  attr_reader :name     # string
  attr_reader :artist   # reference
  attr_reader :duration # integer
  attr_reader :release  # time
  attr_reader :genre    # string
  
  def initialize(name, artist, duration, release)
    @name = name
    @artist = artist
    @duration = duration
    @release = release
    @genre = 'INDIE'
    artist.add_song(self)
  end
end

$songs_sdl = %^
type Query @ruby(class: "Query") {
  artist(name: String!): Artist
}

type Artist @ruby(class: "Artist") {
  name: String!
  songs: [Song]
  origin: [String]
  genre_songs(genre:Genre): [Song]
  songs_after(time: Time): [Song]
}

type Song @ruby(class: "Song") {
  name: String!
  artist: Artist
  duration: Int
  genre: Genre
  release: Time
}

enum Genre {
  POP
  ROCK
  INDIE
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
    Song.new('Jennifer', artist, 240, Time.utc(2017, 5, 5))
    Song.new('Lucky Girl', artist, 170, Time.utc(2017, 5, 5))
    Song.new('Friends', artist, 194, Time.utc(2017, 5, 5))
    Song.new('Reel', artist, 193, Time.utc(2015, 11, 2))
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

type Artist @ruby(class: "Artist") {
  name: String!
  songs: [Song]
  origin: [String]
  genre_songs(genre: Genre): [Song]
  songs_after(time: Time): [Song]
}

type Mutation {
}

type Query @ruby(class: "Query") {
  artist(name: String!): Artist
}

type Song @ruby(class: "Song") {
  name: String!
  artist: Artist
  duration: Int
  genre: Genre
  release: Time
}

type Subscription {
}

enum Genre {
  POP
  ROCK
  INDIE
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
=begin
      get_query_test
      variable_query_test
      json_vars_query_test
      alias_query_test
      list_query_test
      array_query_test # returns an array

      post_graphql_test
      post_json_test
      post_fragment_test
      post_inline_test
      post_nested_test
      post_skip_test
      post_variables_test
      post_enum_test
=end
      post_time_test

      # TBD list arg
      # TBD obj arg
      # TBD time arg and out

      # TBD introspection

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

  def post_graphql_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^{
  artist(name:"Fazerdaze"){
    name
  }
}
^
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end

  def post_json_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^{
  "query":"{\\n  artist(name:\\\"Fazerdaze\\\"){\\n    name\\n  }\\n}"
}^
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^

    post_test(uri, body, 'application/json', expect)
  end

  def post_fragment_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
{
  artist(name:"Fazerdaze") {
    ...basic
  }
}

fragment basic on Artist {
  name
  origin
}
^
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

    post_test(uri, body, 'application/graphql', expect)
  end

  def post_inline_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
{
  artist(name:"Fazerdaze") {
    ... on Artist {
      name
      origin
    }
  }
}
^
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

    post_test(uri, body, 'application/graphql', expect)
  end

  def post_skip_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
query skippy($boo: Boolean = true){
  artist(name:"Fazerdaze") {
    ... @include(if: true) {
      name
    }
    ... @skip(if: true) {
      origin
    }
  }
}
^
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end

  def post_nested_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
query skippy($boo: Boolean = true){
  artist(name:"Fazerdaze") {
    name
    songs {
      name
      duration
    }
  }
}
^
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze",
      "songs":[
        {
          "name":"Jennifer",
          "duration":240
        },
        {
          "name":"Lucky Girl",
          "duration":170
        },
        {
          "name":"Friends",
          "duration":194
        },
        {
          "name":"Reel",
          "duration":193
        }
      ]
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end

  def post_variables_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
query skippy($boo: Boolean = true){
  artist(name:"Fazerdaze") {
    ... @include(if: $boo) {
      name
    }
    ... @skip(if: $boo) {
      origin
    }
  }
}
^
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end

  def post_enum_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
{
  artist(name:"Fazerdaze") {
    name
    songs: genre_songs(genre: INDIE) {
      name
      genre
    }
  }
}
^
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze",
      "songs":[
        {
          "name":"Jennifer",
          "genre":"INDIE"
        },
        {
          "name":"Lucky Girl",
          "genre":"INDIE"
        },
        {
          "name":"Friends",
          "genre":"INDIE"
        },
        {
          "name":"Reel",
          "genre":"INDIE"
        }
      ]
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end

  def post_time_test
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
{
  artist(name:"Fazerdaze") {
    songs: songs_after(time: "2016-01-01T00:00:00.000000000Z") {
      name
      release
    }
  }
}
^
    expect = %^{
  "data":{
    "artist":{
      "songs":[
        {
          "name":"Jennifer",
          "release":"2017-05-05T00:00:00.000000000Z"
        },
        {
          "name":"Lucky Girl",
          "release":"2017-05-05T00:00:00.000000000Z"
        },
        {
          "name":"Friends",
          "release":"2017-05-05T00:00:00.000000000Z"
        },
        {
          "name":"Reel",
          "release":"2015-11-02T00:00:00.000000000Z"
        }
      ]
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end

  ##################################
  
  def req_test(uri, expect)
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal(expect, content)
  end

  def post_test(uri, body, content_type, expect)
    uri = URI(uri)
    req = Net::HTTP::Post.new(uri)
    req['Accept-Encoding'] = '*'
    req['Content-Type'] = content_type
    req.body = body
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    assert_equal('application/json', res['Content-Type'])
    assert_equal(expect, content)
  end
end
