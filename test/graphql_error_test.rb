#!/usr/bin/env ruby

$: << File.dirname(__FILE__)
$root_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
%w(lib ext).each do |dir|
  $: << File.join($root_dir, dir)
end

require 'minitest'
require 'minitest/autorun'

require 'agoo'

class Schema
  attr_reader :query

  def initialize()
  end
end

class GraphQLErrTest < Minitest::Test

  def test_bad_sdl
    bad_sdl = %^
type Query @ruby(class: "Query" {
}
^
    Agoo::GraphQL.schema(Schema.new) {
      assert_raises(StandardError) {
	Agoo::GraphQL.load(bad_sdl)
      }
    }
  end

end
