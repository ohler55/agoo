
module Agoo
end

require 'stringio' # needed for Ruby 2.7
require 'agoo/version'
require 'agoo/graphql'
require 'rack/handler/agoo'
require 'agoo/agoo' # C extension

ENV['RACKUP_HANDLER'] = 'Agoo'
ENV['RACK_HANDLER'] = 'Agoo'
