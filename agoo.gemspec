
require 'date'
require File.join(File.dirname(__FILE__), 'lib/agoo/version')

Gem::Specification.new do |s|
  s.name = "agoo"
  s.version = Agoo::VERSION
  s.authors = "Peter Ohler"
  s.date = Date.today.to_s
  s.email = "peter@ohler.com"
  s.homepage = "http://www.ohler.com/agoo"
  s.summary = "An HTTP server"
  s.description = "A fast HTTP server supporting rack."
  s.licenses = ['MIT']
  s.required_ruby_version = ">= 2.0"

  s.files = Dir["{lib,ext,test}/**/*.{rb,h,c}"] + ['LICENSE', 'README.md']
  s.test_files = Dir["test/**/*.rb"]
  s.extensions = ["ext/agoo/extconf.rb"]

  s.has_rdoc = true
  s.extra_rdoc_files = ['README.md'] + Dir["pages/*.md"]
  s.rdoc_options = ['-t', 'Agoo', '-m', 'README.md', '-x', 'test/*']

  s.rubyforge_project = 'agoo'

  s.add_development_dependency 'rake-compiler', '>= 0.9', '< 2.0'
  s.add_development_dependency 'minitest', '~> 5'
  s.add_development_dependency 'test-unit', '~> 3.0'
  s.add_development_dependency 'wwtd', '~> 0'
end
