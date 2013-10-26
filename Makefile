all install uninstall clean:
	cd src && $(MAKE) $@

TAG = $(shell git describe --abbrev=0 --tags)
VERSION = $(shell echo $(TAG) | sed s/^v//)
FORMAT = tar.gz

dist:
	@ if [ -n "`git tag --list $(TAG)`" ]; \
	then \
		git archive --verbose --format=$(FORMAT) \
		--prefix=imapfilter-$(VERSION)/ \
		--output=imapfilter-$(VERSION).$(FORMAT) v$(VERSION); \
		echo "Created Git archive: imapfilter-$(VERSION).$(FORMAT)"; \
	else \
		echo "No such tag in the Git repository: $(TAG)"; \
	fi

distclean:
	rm -f imapfilter-*.*
