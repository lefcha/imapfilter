DESTDIR =
BINDIR = /usr/local/bin
SHAREDIR = /usr/local/share/imapfilter
MANDIR = /usr/local/man

INCDIRS = -I/usr/local/include
LIBDIRS = -L/usr/local/lib

MYCFLAGS =  -Wall -O
MYLDFLAGS = 

DEFS = -DMAKEFILE_SHAREDIR='"$(SHAREDIR)"'

CFLAGS = $(MYCFLAGS) $(DEFS) $(INCDIRS)
LDFLAGS = $(MYLDFLAGS) $(LIBDIRS)

LIBS = -lm -llua -lpcre -lssl -lcrypto

MAN_BIN = imapfilter.1
MAN_CONFIG = imapfilter_config.5

COMMON_LUA = common.lua
SET_LUA = set.lua
REGEX_LUA = regex.lua
ACCOUNT_LUA = account.lua
MAILBOX_LUA = mailbox.lua
MESSAGE_LUA = message.lua
OPTIONS_LUA = options.lua
AUXILIARY_LUA = auxiliary.lua
DEPRECATED_LUA = deprecated.lua

BIN = imapfilter
OBJ = auth.o buffer.o cert.o core.o file.o imap.o imapfilter.o list.o log.o \
	lua.o memory.o misc.o namespace.o pcre.o regexp.o request.o \
	response.o session.o signal.o socket.o system.o

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) -o $(BIN) $(LDFLAGS) $(OBJ) $(LIBS)

$(OBJ): imapfilter.h
buffer.o imap.o imapfilter.o namespace.o request.o response.o: buffer.h
cert.o file.o imapfilter.o log.o lua.o: pathnames.h
imapfilter.o log.o session.o: list.h
imapfilter.o regexp.o response.o: regexp.h
auth.o cert.o imap.o imapfilter.o log.o request.o response.o session.o \
	socket.o: session.h
imapfilter.o: version.h

install: $(BIN)
	if test ! -d $(DESTDIR)$(BINDIR); then \
		mkdir -p $(DESTDIR)$(BINDIR); fi
	cp -f $(BIN) $(DESTDIR)$(BINDIR) && \
		chmod 0755 $(DESTDIR)$(BINDIR)/$(BIN)
	if test ! -d $(DESTDIR)$(SHAREDIR); then \
		mkdir -p $(DESTDIR)$(SHAREDIR); fi
	cp -f $(COMMON_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(COMMON_LUA)
	cp -f $(SET_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(SET_LUA)
	cp -f $(REGEX_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(REGEX_LUA)
	cp -f $(ACCOUNT_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(ACCOUNT_LUA)
	cp -f $(MAILBOX_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(MAILBOX_LUA)
	cp -f $(MESSAGE_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(MESSAGE_LUA)
	cp -f $(OPTIONS_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(OPTIONS_LUA)
	cp -f $(AUXILIARY_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(AUXILIARY_LUA)
	cp -f $(DEPRECATED_LUA) $(DESTDIR)$(SHAREDIR) && \
		chmod 0644 $(DESTDIR)$(SHAREDIR)/$(DEPRECATED_LUA)
	if test ! -d $(DESTDIR)$(MANDIR)/man1; then \
		mkdir -p $(DESTDIR)$(MANDIR)/man1; fi
	cp -f ../doc/$(MAN_BIN) $(DESTDIR)$(MANDIR)/man1 && \
		chmod 0644 $(DESTDIR)$(MANDIR)/man1/$(MAN_BIN)
	if test ! -d $(DESTDIR)$(MANDIR)/man5; then \
		mkdir -p $(DESTDIR)$(MANDIR)/man5; fi
	cp -f ../doc/$(MAN_CONFIG) $(DESTDIR)$(MANDIR)/man5 && \
		chmod 0644 $(DESTDIR)$(MANDIR)/man5/$(MAN_CONFIG)

deinstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN) \
		$(DESTDIR)$(SHAREDIR)/$(COMMON_LUA) \
		$(DESTDIR)$(SHAREDIR)/$(SET_LUA) \
		$(DESTDIR)$(SHAREDIR)/$(REGEX_LUA) \
		$(DESTDIR)$(SHAREDIR)/$(ACCOUNT_LUA) \
		$(DESTDIR)$(SHAREDIR)/$(MAILBOX_LUA) \
		$(DESTDIR)$(SHAREDIR)/$(MESSAGE_LUA) \
		$(DESTDIR)$(SHAREDIR)/$(OPTIONS_LUA) \
		$(DESTDIR)$(SHAREDIR)/$(AUXILIARY_LUA) \
		$(DESTDIR)$(SHAREDIR)/$(DEPRECATED_LUA) \
		$(DESTDIR)$(MANDIR)/man1/$(MAN_BIN) \
		$(DESTDIR)$(MANDIR)/man5/$(MAN_CONFIG)

uninstall: deinstall

clean:
	rm -f $(OBJ) $(BIN) imapfilter.core core *.orig *.BAK *~

distclean: clean
	@if test -f .Makefile; then mv -f .Makefile Makefile; fi
