#!/usr/bin/env ruby

$: << File.dirname(__FILE__)
$root_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
%w(lib ext).each do |dir|
  $: << File.join($root_dir, dir)
end

require 'minitest'
require 'minitest/autorun'
require 'net/http'

require 'oj'
require 'agoo'

class Artist
  attr_reader :name
  attr_reader :songs
  attr_reader :origin
  attr_reader :likes
  
  def initialize(name, origin)
    @name = name
    @songs = []
    @origin = origin
    @likes = 0
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

  def like
    @likes += 1
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

type Mutation {
  like(artist: String!): Artist
}

type Artist @ruby(class: "Artist") {
  name: String!
  songs: [Song]
  origin: [String]
  genre_songs(genre:Genre): [Song]
  songs_after(time: Time): [Song]
  likes: Int
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

class Mutation
  attr_reader :artists

  def initialize(artists)
    @artists = artists
  end

  def like(args={})
    artist = @artists[args['artist']]
    artist.like
    artist
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
    @mutation = Mutation.new(@artists)
  end
end

class GraphQLTest < Minitest::Test
  @@server_started = false
  
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
  likes: Int
}

type Mutation {
  like(artist: String!): Artist
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

  def start_server
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

    Agoo::GraphQL.schema(Schema.new) {
      Agoo::GraphQL.load($songs_sdl)
    }

    @@server_started = true
  end
  
  def setup
    if @@server_started == false
      start_server
    end
  end

  Minitest.after_run {
    GC.start
    Agoo::shutdown
  }

  # TBD list arg
  # TBD obj arg
  # TBD introspection

  ##################################
  
  def test_load
    content = Agoo::GraphQL.sdl_dump(with_descriptions: false, all: false)
    content.force_encoding('UTF-8')
    assert_equal(SCHEMA_EXPECT, content)
  end

  def test_get_schema
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

  def test_get_query
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
  
  def test_variable_query
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
  
  def test_alias_query
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
  
  def test_parse_error
    uri = URI('http://localhost:6472/graphql?query=nonsense')
    expect = %^{
  "errors":[
    {
      "message":"unexpected character at 1:1",
      "code":"parse error"
    }
  ]
}
^
    req_test(uri, expect, 'errors.0.timestamp')
  end
  
  def test_json_vars_query
    #uri = URI('http://localhost:6472/graphql?query={artist(name:$name){name}}&indent=2&variables={"name":"Fazerdaze"}')
    uri = URI('http://localhost:6472/graphql?query={artist(name:"Fazerdaze"){name}}&indent=2&variables={"name":"Fazerdaze"}')
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze"
    }
  }
}^
    req_test(uri, expect)
  end
  
  def test_list_query
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

  def test_array_query
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

  def test_post_graphql
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

  def test_post_json
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

  def test_post_fragment
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

  def test_post_inline
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

  def test_post_skip
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

  def test_post_nested
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
query skippy($boo: Boolean = true){
  artist(name:"Fazerdaze") {
    name
    __typename
    songs {
      name
      __typename
      duration
    }
  }
}
^
    expect = %^{
  "data":{
    "artist":{
      "name":"Fazerdaze",
      "__typename":"Artist",
      "songs":[
        {
          "name":"Jennifer",
          "__typename":"Song",
          "duration":240
        },
        {
          "name":"Lucky Girl",
          "__typename":"Song",
          "duration":170
        },
        {
          "name":"Friends",
          "__typename":"Song",
          "duration":194
        },
        {
          "name":"Reel",
          "__typename":"Song",
          "duration":193
        }
      ]
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end

  def test_post_variables
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

  def test_post_enum
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

  def test_post_time
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
        }
      ]
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end

  def test_intro_type
    uri = URI('http://localhost:6472/graphql?query={__type(name:"Artist"){kind,name,description}}&indent=2')
    expect = %^{
  "data":{
    "__type":{
      "kind":"OBJECT",
      "name":"Artist",
      "description":null
    }
  }
}^
    req_test(uri, expect)
  end
  
  def test_intro_of_type
    uri = URI('http://localhost:6472/graphql?query={__type(name:"[__Type!]"){name,ofType{name}}}&indent=2')
    expect = %^{
  "data":{
    "__type":{
      "name":"[__Type!]",
      "ofType":{
        "name":"__Type"
      }
    }
  }
}^
    req_test(uri, expect)
  end

  def test_intro_enum_values
    uri = URI('http://localhost:6472/graphql?query={__type(name:"__TypeKind"){name,choices:enumValues{name,isDeprecated}}}&indent=2')
    expect = %^{
  "data":{
    "__type":{
      "name":"__TypeKind",
      "choices":[
        {
          "name":"SCALAR",
          "isDeprecated":false
        },
        {
          "name":"OBJECT",
          "isDeprecated":false
        },
        {
          "name":"INTERFACE",
          "isDeprecated":false
        },
        {
          "name":"UNION",
          "isDeprecated":false
        },
        {
          "name":"ENUM",
          "isDeprecated":false
        },
        {
          "name":"INPUT_OBJECT",
          "isDeprecated":false
        },
        {
          "name":"LIST",
          "isDeprecated":false
        },
        {
          "name":"NON_NULL",
          "isDeprecated":false
        }
      ]
    }
  }
}^
    req_test(uri, expect)
  end

  def test_intro_fields
    uri = URI('http://localhost:6472/graphql?query={__type(name:"Artist"){name,fields{name,type{name},args{name,type{name},defaultValue}}}}&indent=2')
    expect = %^{
  "data":{
    "__type":{
      "name":"Artist",
      "fields":[
        {
          "name":"name",
          "type":{
            "name":"String"
          },
          "args":[
          ]
        },
        {
          "name":"songs",
          "type":{
            "name":"[Song]"
          },
          "args":[
          ]
        },
        {
          "name":"origin",
          "type":{
            "name":"[String]"
          },
          "args":[
          ]
        },
        {
          "name":"genre_songs",
          "type":{
            "name":"[Song]"
          },
          "args":[
            {
              "name":"genre",
              "type":{
                "name":"Genre"
              },
              "defaultValue":null
            }
          ]
        },
        {
          "name":"songs_after",
          "type":{
            "name":"[Song]"
          },
          "args":[
            {
              "name":"time",
              "type":{
                "name":"Time"
              },
              "defaultValue":null
            }
          ]
        },
        {
          "name":"likes",
          "type":{
            "name":"Int"
          },
          "args":[
          ]
        }
      ]
    }
  }
}^
    req_test(uri, expect)
  end
  
  def test_intro_schema_types
    uri = URI('http://localhost:6472/graphql?query={__schema{types{name}}}&indent=2')
    expect = %^{
  "data":{
    "__schema":{
      "types":[
        {
          "name":"__EnumValue"
        },
        {
          "name":"Int"
        },
        {
          "name":"Subscription"
        },
        {
          "name":"I64"
        },
        {
          "name":"__DirectiveLocation"
        },
        {
          "name":"Time"
        },
        {
          "name":"Genre"
        },
        {
          "name":"__Schema"
        },
        {
          "name":"Mutation"
        },
        {
          "name":"Uuid"
        },
        {
          "name":"Boolean"
        },
        {
          "name":"schema"
        },
        {
          "name":"String"
        },
        {
          "name":"Song"
        },
        {
          "name":"Artist"
        },
        {
          "name":"__Directive"
        },
        {
          "name":"__Field"
        },
        {
          "name":"__TypeKind"
        },
        {
          "name":"Query"
        },
        {
          "name":"__InputValue"
        },
        {
          "name":"__Type"
        },
        {
          "name":"Float"
        }
      ]
    }
  }
}^
    req_test(uri, expect)
  end

  def test_intro_schema_query_type
    uri = URI('http://localhost:6472/graphql?query={__schema{queryType{name}}}&indent=2')
    expect = %^{
  "data":{
    "__schema":{
      "queryType":{
        "name":"Query"
      }
    }
  }
}^
    req_test(uri, expect)
  end
  
  def test_intro_schema_directives
    uri = URI('http://localhost:6472/graphql?query={__schema{directives{name,locations,args{name}}}}&indent=2')
    expect = %^{
  "data":{
    "__schema":{
      "directives":[
        {
          "name":"ruby",
          "locations":[
            "SCHEMA",
            "OBJECT"
          ],
          "args":[
            {
              "name":"class"
            }
          ]
        },
        {
          "name":"skip",
          "locations":[
            "FIELD",
            "FRAGMENT_SPREAD",
            "INLINE_FRAGMENT"
          ],
          "args":[
            {
              "name":"if"
            }
          ]
        },
        {
          "name":"include",
          "locations":[
            "FIELD",
            "FRAGMENT_SPREAD",
            "INLINE_FRAGMENT"
          ],
          "args":[
            {
              "name":"if"
            }
          ]
        },
        {
          "name":"deprecated",
          "locations":[
            "FIELD_DEFINITION",
            "ENUM_VALUE"
          ],
          "args":[
            {
              "name":"reason"
            }
          ]
        }
      ]
    }
  }
}^
    req_test(uri, expect)
  end
  
  def test_mutation
    uri = URI('http://localhost:6472/graphql?indent=2')
    body = %^
mutation {
  like(artist:"Fazerdaze") {
    name
    likes
  }
}
^
    expect = %^{
  "data":{
    "like":{
      "name":"Fazerdaze",
      "likes":1
    }
  }
}^

    post_test(uri, body, 'application/graphql', expect)
  end



  ##################################

  def deep_delete(obj, path)
    key = path[0]
    if 1 == path.length
      obj.delete(key)
    else
      if key == key.to_i.to_s
	deep_delete(obj[key.to_i], path[1..-1])
      else
	deep_delete(obj[key], path[1..-1])
      end
    end
  end
  
  def req_test(uri, expect, ignore=nil)
    req = Net::HTTP::Get.new(uri)
    res = Net::HTTP.start(uri.hostname, uri.port) { |h|
      h.request(req)
    }
    content = res.body
    unless ignore.nil?
      result = Oj.load(content, mode: :strict)
      deep_delete(result, ignore.split('.'))
      content = Oj.dump(result, indent: 2)
    end
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
