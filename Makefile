CFLAGS=
GTK=`pkg-config --cflags --libs gtk+-3.0`
PREFIX=/usr

all : iconify

iconify: bin mo
	

mo:
	mkdir -p mo/fr/LC_MESSAGES
	msgfmt -co mo/fr/LC_MESSAGES/iconify.mo po/fr.po

bin: iconify.o
	gcc iconify.o -O3 -o iconify $(CFLAGS) $(GTK)

clean:
	rm -f iconify iconify.o
	rm -r mo

%.o: src/%.c
	gcc -c -g -Wall -O3 $(CFLAGS) $< $(GTK)

install:
	cp iconify $(PREFIX)/bin/
	chmod a+x $(PREFIX)/bin/iconify
	cp data/iconify.png $(PREFIX)/share/pixmaps/
	chmod a+r $(PREFIX)/share/pixmaps/iconify.png
	cp -R mo/* $(PREFIX)/share/locale
	chmod a+r $(PREFIX)/share/locale/**/LC_MESSAGES/iconify.mo

uninstall:
	rm $(PREFIX)/bin/iconify
	rm $(PREFIX)/share/pixmaps/iconify.png
	rm $(PREFIX)/share/locale/**/LC_MESSAGES/iconify.mo
