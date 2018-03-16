
module Agoo
end

require 'agoo/version'
require 'rack/handler/agoo'
require 'agoo/agoo' # C extension

ENV['RACK_HANDLER'] = 'Agoo'
