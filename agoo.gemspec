
require 'date'
require File.join(File.dirname(__FILE__), 'lib/agoo/version')

Gem::Specification.new do |s|
  s.name = 'agoo'
  s.version = Agoo::VERSION
  s.authors = 'Peter Ohler'
  s.date = Date.today.to_s
  s.email = 'peter@ohler.com'
  s.homepage = 'https://github.com/ohler55/agoo'
  s.summary = 'An HTTP server'
  s.description = 'A fast HTTP server supporting rack.'
  s.licenses = ['MIT']
  s.metadata = {
    'changelog_uri' => 'https://github.com/ohler55/agoo/CHANGELOG.md',
    'source_code_uri' => 'https://github.com/ohler55/agoo',
    'homepage' = 'https://github.com/ohler55/agoo'
  }

  s.files = Dir["{lib,ext,test}/**/*.{rb,h,c}"] + ['LICENSE', 'README.md', 'CHANGELOG.md']
  s.test_files = Dir["test/**/*.rb"]
  s.extensions = ["ext/agoo/extconf.rb"]

  s.extra_rdoc_files = ['README.md', 'CHANGELOG.md', 'LICENSE'] + Dir["pages/*.md"]
  s.rdoc_options = ['-t', 'Agoo', '-m', 'README.md', '-x', '"test/*"', '-x', '"example/*"', '-x', 'extconf.rb']

  s.bindir = 'bin'
  s.executables << 'agoo'
  s.executables << 'agoo_stubs'

  s.required_ruby_version = '>= 2.5'
  s.requirements << 'Linux or macOS'

  s.add_development_dependency 'oj', '~> 3.10', '>= 3.10.0'

end
