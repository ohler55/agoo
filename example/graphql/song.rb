
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

Agoo::Server.init(6464, 'root', thread_count: 1, graphql: '/graphql')

Agoo::Server.start()

# Define the API using GraphQL SDL. Use the Agoo specific directive @ruby to
# link the GraphQL type to a Ruby class. This is only needed when using
# fragments.
$songs_sdl = %^
type Query @ruby(class: "Query") {
  artist(name: String!): Artist
}

type Artist @ruby(class: "Artist") {
  name: String!
  songs: [Song]
  origin: [String]
}

type Song @ruby(class: "Song") {
  name: String!
  artist: Artist
  duration: Int
  release: String
}
^

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

# This is the class that implemnts the root query operation.
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

# Ready to load up the SDL. This also validates the SDL.
Agoo::GraphQL.schema(Schema.new) {
  Agoo::GraphQL.load($songs_sdl)
}

# When starting with a thread_count over 0 just sleep until a ^C is
# signalled. Agoo must be running to load the SDL.
sleep

puts %^open 'localhost:6464/graphql?query={artist(name:"Fazerdaze"){name}}&indent=2' in a browser.^

# To run this example type the following then go to a browser and enter a URL
# of
# localhost:6464/graphql?query={artist(name:"Fazerdaze"){name}}&indent=2. For
# a more complex query try
# localhost:6464/graphql?query={artist(name:"Fazerdaze"){name,songs{name,duration}}}&indent=2.

# ruby song.rb
