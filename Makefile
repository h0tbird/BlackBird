#------------------------------------------------------------------------------
# exports:
#------------------------------------------------------------------------------

export CFLAGS= -Wall -Werror
export LFLAGS= -pthread
export bin-dir= $(basedir)/usr/sbin
export cfg-dir= $(basedir)/etc/BlackBird

#------------------------------------------------------------------------------
# all:
#------------------------------------------------------------------------------

all:
	make -C src

#------------------------------------------------------------------------------
# clean:
#------------------------------------------------------------------------------

clean:
	make -C src clean

#------------------------------------------------------------------------------
# install:
#------------------------------------------------------------------------------

install:
	make -C src install
	mkdir -p $(cfg-dir)