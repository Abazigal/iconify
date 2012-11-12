CFLAGS=-Wall -O3
GTK=`pkg-config --cflags --libs gtk+-3.0`

PREFIX=/usr
LOCALE_DIR = $(PREFIX)/share/locale
ICON_DIR = $(PREFIX)/share/pixmaps
BIN_DIR = $(PREFIX)/bin

PO_DIR = po
PO = $(wildcard $(PO_DIR)/*.po)
MO = $(PO:%.po=%.mo)


all : iconify

iconify: iconify.o
	gcc iconify.o -o iconify $(CFLAGS) $(GTK)


%.o: src/%.c
	gcc -c $(CFLAGS) $< $(GTK)

$(MO): $(PO_DIR)/%.mo: $(PO_DIR)/%.po
	@echo "formatting '$*.po'"
	@msgfmt $(PO_DIR)/$*.po -o $@



install: install_bin install_icon install_mo

install_bin: iconify
	@echo "installing iconify to '$(BIN_DIR)'"
	@install -d -m 755 $(BIN_DIR)
	@install -m 755 iconify $(BIN_DIR)

install_icon:
	@echo "Installing icon to '$(ICON_DIR)'"
	@install -d $(ICON_DIR)
	@install -m 644 data/iconify.png $(ICON_DIR)

install_mo: $(MO)
	@echo "installing *.mo files to '$(LOCALE_DIR)'"
	@for i in `ls $(PO_DIR)/*.mo` ; do \
		mkdir -p $(LOCALE_DIR)/`echo $$i | grep -o [^/]*$$ | sed -e s/.mo//`/LC_MESSAGES;\
		install -c -m644 $$i $(LOCALE_DIR)/`echo $$i | grep -o [^/]*$$ | sed -e s/.mo//`/LC_MESSAGES/iconify.mo ; \
	done


clean:
	@echo "cleaning up"
	@rm -f iconify
	@rm -f $(PO_DIR)/*.mo
	@rm -f *.o

uninstall:
	@echo "uninstalling"
	@rm $(BIN_DIR)/iconify
	@rm $(ICON_DIR)/iconify.png
	@for i in `ls $(PO_DIR)/*.po` ; do \
		rm -f $(LOCALE_DIR)/`echo $$i | grep -o [^/]*$$ | sed -e s/.po//`/LC_MESSAGES/iconify.mo || test -z "" ; \
	done
