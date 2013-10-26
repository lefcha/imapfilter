all install uninstall clean:
	cd src && $(MAKE) $@

tarbz2 :
	./mkpackage.sh `git describe --tags HEAD`
