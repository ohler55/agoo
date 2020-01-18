
require 'date'

require 'agoo'

Agoo::Log.configure(dir: '',
		    console: true,
		    classic: true,
		    colorize: true,
		    states: {
		      INFO: true,
		      DEBUG: false,
		      connect: true,
		      request: true,
		      response: true,
		      eval: true,
		      push: false,
		    })

# Next implement the Ruby classes to support the API. The type and class names
# are the same in this example to make it easier to follow.
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
    n = args['name']
    @songs.each { |s| return s if n = s.name }
    nil
  end

end

class Song
  attr_reader :name     # string
  attr_reader :artist   # reference
  attr_reader :duration # integer
  attr_reader :release  # date
  attr_accessor :likes  # integer

  def initialize(name, artist, duration, release)
    @name = name
    @artist = artist
    @duration = duration
    @release = release
    @likes = 0
    artist.songs << self
  end
end

# This is the class that implements the root query operation.
class Query
  attr_reader :artists

  def initialize(artists)
    @artists = artists
  end

  def artist(args={})
    n = args['name']
    @artists.each { |a| return a if n = a.name }
    nil
  end
end

class Mutation
  def initialize(artists)
    @artists = artists
    @lock = Mutex.new
  end

  def like(args={})
    an = args['artist']
    sn = args['song']
    @lock.synchronize {
      @artists.each {|a|
	 if an == a.name
	   a.songs.each { |s| if s.name == sn; s.likes += 1; return s; end }
	 end
      }
    }
    nil
  end

end

class Schema
  attr_reader :query
  attr_reader :mutation
  attr_reader :subscription

  def initialize(query)
    @query = query
    @mutation = Mutation.new(query.artists)
  end
end

# Populate the library.
fazerdaze = Artist.new('Fazerdaze', ['Morningside', 'Auckland', 'New Zealand'])
Song.new('Jennifer', fazerdaze, 240, Date.new(2017, 5, 5))
Song.new('Lucky Girl', fazerdaze, 170, Date.new(2017, 5, 5))
Song.new('Friends', fazerdaze, 194, Date.new(2017, 5, 5))
Song.new('Reel', fazerdaze, 193, Date.new(2015, 11, 2))

boys = Artist.new('Viagra Boys', ['Stockholm', 'Sweden'])
Song.new('Down In The Basement', boys, 216, Date.new(2018, 9, 28))
Song.new('Frogstrap', boys, 195, Date.new(2018, 9, 28))
Song.new('Worms', boys, 208, Date.new(2018, 9, 28))
Song.new('Amphetanarchy', boys, 346, Date.new(2018, 9, 28))

$schema = Schema.new(Query.new([fazerdaze, boys]))

puts %^\nopen 'localhost:6464/graphql?query={artist(name:"Fazerdaze"){name,songs{name,duration}}}&indent=2' in a browser.\n\n^
#http://localhost:6464/graphql?query={artists{name,origin,songs{name,duration,likes}},__schema{types{name,kind,fields{name}}}}

Agoo::Server.init(6464, 'root', thread_count: 1, graphql: '/graphql')
Agoo::Server.start
Agoo::GraphQL.schema($schema) {Agoo::GraphQL.load_file('song.graphql')}
# Use CORS to allow GraphiQL to connect.
Agoo::GraphQL.build_headers = proc{ |req|
  origin = req.headers['HTTP_ORIGIN'] || '*'
  {
    'Access-Control-Allow-Origin' => origin,
    'Access-Control-Allow-Headers' => '*',
    'Access-Control-Allow-Credentials' => true,
    'Access-Control-Max-Age' => 3600,
  }
}

# When starting with a thread_count over 0 just sleep until a ^C is
# signalled. Agoo must be running to load the SDL.
sleep

# To run this example type the following then go to a browser and enter a URL of
# localhost:6464/graphql?query={artist(name:"Fazerdaze"){name}}&indent=2. For
# a more complex query try
# localhost:6464/graphql?query={artist(name:"Fazerdaze"){name,songs{name,duration}}}&indent=2.

# ruby song.rb
