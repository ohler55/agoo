require 'mkmf'
require 'rbconfig'

extension_name = 'agoo'
dir_config(extension_name)

$CPPFLAGS += " -DPLATFORM_LINUX" if 'x86_64-linux' == RUBY_PLATFORM
# Travis defaults to an older version of gcc to force a newer version.
#RbConfig::MAKEFILE_CONFIG['CC'] = "gcc-7" if 'x86_64-linux' == RUBY_PLATFORM

CONFIG['warnflags'].slice!(/ -Wsuggest-attribute=format/)

create_makefile(File.join(extension_name, extension_name))

puts ">>>>> Created Makefile for #{RUBY_DESCRIPTION.split(' ')[0]} version #{RUBY_VERSION} on #{RUBY_PLATFORM} <<<<<"

%x{make clean}
