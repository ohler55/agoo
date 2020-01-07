# GraphQL with a Song

You've may have heard developer singing praises about how wonderful GraphQL
is. Maybe you thought it was looking into. I like to learn new technologies by
using them so this article is a simple example of using GraphQL.

GraphQL is a language for describing an applicaiton API. It holds a similar
position in the development stack as a REST API but with more
flexibility. Unlike REST, GraphQL allows response formats and content to be
specified by the client. Just as SQL `SELECT` statements allow query results
to be specified, GraphQL allows returned JSON data structure to be
specified. Following the SQL analogy GraphQL does not provide a `WHERE` clause
but identifies fields on application objects that should provide the data for
the response.

GraphQL, as the name suggests models APIs as if the application is a graph of
data. While that description may not be how you have viewed your application
it is a model used in most systems. If your application is object based then
the objects refer to other objects. These object-method-object define a
graph. Data that can be represented by JSON is a graph as JSON is just a
directed graph. Thinking about the application as presenting a graph model
through the API will make GraphQL much easier to understand.

## Consider the Application

Enough of the abstract. Lets get down to actually building an application that
uses GraphQL by starting with a definition of the data model or the
graph. Last year I picked up a new hobby. I'm learning to play the electric
upright bass and about music so a music related example came to mind when
coming up with and example.

Keeping it simple the object types are __Artist__ and __Song__. __Artists__
have multiple __Song__ and a __Song__ is associate with an __Artist__. Each
object type has attributes such as a `name`. Pretty basic and simple.

## Define the API

GraphQL uses SDL (Schema Definition Language) which is sometimes refered to as
"type system definition language" in the GraphQL specification. GraphQL types
can, in theory be defined in any language but most common language agnostic
language is SDL so lets use SDL to define the API.

```
type Artist {
  name: String!
  songs: [Song]
  origin: [String]
}

type Song {
  name: String!
  artist: Artist
  duration: Int
  release: String
}
```

Pretty easy to understand. An __Artist__ has a `name` that is a `String`,
`songs` that is an array of __Song__ objects, and `origin` which is a `String`
array. __Song__ is similar but with one odd field. The `release` field should
be a time or date type but GraphQL does not have that type defined as a core
type. To be completely portable between any GraphQL implementation a `String`
is used. The GraphQL implemenation we will use as added the `Time` type so
lets change the __Song__ definition so that the `release` field is a `Time`
type. The returned value will be a `String` but by setting the type to `Time`
we document the API more accurately.

```
  release: Time
```

The last step is to describe how to get one or more of the objects. This is
referred to as the root or for queries the query root. Our root will have just
one field or method called `artist` and will require an artist `name`.

```
type Query {
  artist(name: String!): Artist
}
```

## Writing the Application

Ruby will be used write the application. There is more than one implemenation
of a GraphQL server for Ruby. Some approachs require the SDL above to be
translated into a Ruby equivalent. [Agoo](https://github.com/ohler55/agoo)
uses the SDL defintion as it is and the Ruby code is plain vanilla Ruby so
that will be used for this example.

By having the Ruby class names match the GraphQL type names we keep things
simple. Note that the Ruby classes match the GraphQL types.

```ruby
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
```

Method match fields in the Ruby classes. Note the method all have
either no arguments or `args={}`. This is what the GraphQL APIs expect
and the [Agoo](https://github.com/ohler55/agoo) GraphQL implementation
follows suit. [Agoo](https://github.com/ohler55/agoo) offers
additional options that are available based on the method
signature. If a method signature is `(args, req)` or `(args, req,
plan)` then the request object is included. The plan, if included, is
an object that can be queried for information about the query. (It has
not been implemented as of version 2.12.0.)

The `initialize` methods are used to set up the data for this example
as we will see shortly.

The query root class als needs to be defined. Note the `artist` method which
matches the SDL `Query` root type. An `attr_reader` for `artists` was also
added. That would be exposed to the API simply by adding that field to the
`Query` type in the SDL document.

```ruby
class Query
  attr_reader :artists

  def initialize(artists)
    @artists = artists
  end

  def artist(args={})
    @artists[args['name']]
  end
end
```

The GraphQL root, not to be confused with the query root sites above the query
root. GraphQL defines it to optionally have three field. The Ruby class in
this case implements on the `query` field. The initializer loads up some data
for an Indie band from New Zealand I like to listen to.

```ruby
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
```

The final hookup is implemenation specific. For
[Agoo](https://github.com/ohler55/agoo) the server is initialized to include a
handler for the `/graphql` HTTP request path ad then it is started.

```
Agoo::Server.init(6464, 'root', thread_count: 1, graphql: '/graphql')
Agoo::Server.start()
```

The GraphQL implementation is then configured with the SDL defined earlier
(`$songs_sdl`) and then the appliation sleeps while the server processes
requests.

```
Agoo::GraphQL.schema(Schema.new) {
  Agoo::GraphQL.load($songs_sdl)
}
sleep
```

The code for this example is in (https://github.com/ohler55/agoo/example/graphql/song.rb).

## Using the API

A web broswer can be used to try out the API as can `curl`.

The GraphQL query to try looks like the following.

```
{
  artist(name:"Fazerdaze") {
    name
    songs{
      name
      duration
    }
  }
}
```

The query asks for the __Artist__ named `Frazerdaze` and returns the `name` and `songs` in a JSON document. For each __Song__ the `name` and `duration` of the __Song__ is returned in a JSON object for each __Song__. The output should look like this.

```
{
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
}
```

After getting rid of the optional whitespace in the query an HTTP GET made
with curl should return that content.

```
curl -w "\n" 'localhost:6464/graphql?query=\{artist(name:"Fazerdaze")\{name,songs\{name,duration\}\}\}&indent=2'
```

Try changing the query and replace `duration` with `release` and not the
conversion of the Ruby Time to a JSON string.

Hope you enjoyed the example. Now you can sing the GraphQL song.
