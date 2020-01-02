require 'mkmf'
require 'rbconfig'

extension_name = 'agoo'
dir_config(extension_name)
dir_config('openssl')

if 'x86_64-linux' == RUBY_PLATFORM
  $CFLAGS += " -DPLATFORM_LINUX -std=gnu11"
else
  $CFLAGS += ' -std=c11'
end

$CFLAGS += ' -D_POSIX_SOURCE -D_GNU_SOURCE'

# Adding the __attribute__ flag only works with gcc compilers and even then it
# does not work to check args with varargs so just remove the check.
CONFIG['warnflags'].slice!(/ -Wsuggest-attribute=format/)
CONFIG['warnflags'].slice!(/ -Wdeclaration-after-statement/)
CONFIG['warnflags'].slice!(/ -Wmissing-noreturn/)

have_header('stdatomic.h')
#have_header('sys/epoll.h')
have_header('openssl/ssl.h')
have_library('ssl')

create_makefile(File.join(extension_name, extension_name))

puts ">>>>> Created Makefile for #{RUBY_DESCRIPTION.split(' ')[0]} version #{RUBY_VERSION} on #{RUBY_PLATFORM} <<<<<"

%x{make clean}
