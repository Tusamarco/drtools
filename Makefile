CFLAGS=-DHAVE_OFFSET64_T -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1 -D_LARGEFILE_SOURCE=1 -Wall -O3 -g 
# Uncomment to build static binaries
#CFLAGS+=-static -lrt

INCLUDES=-I include -I mysql-source/include -I mysql-source/innobase/include
MYSQLLIB=-L/usr/local/mysql/lib -lmysqlclient -lpthread -lm -lrt -ldl

PREFIX?=/usr/local/bin

BINARIES=constraints_parser page_parser page_printer innochecksum_changer ibdconnect s_tables s_indexes bgrep ibd_rename_ids sys_parser index_chk
all: prerequisites $(BINARIES)

prerequisites:
	@echo "Make sure you have installed following packages:"
	@echo "gcc"
	@echo "libncurses5-dev or ncurses-devel"
	@echo "glibc-static on CentOS"
	@echo "perl-DBD-MySQL"
	@echo ""
	@echo "It's recomended to install"
	@echo "bvi (install Dag's repo if you need http://dag.wieers.com/rpm/FAQ.php#B)"
	@echo "screen"
	@echo "vim"
	@echo ""
	@touch prerequisites

# LIBS

lib: 
	mkdir lib

lib/print_data.o: lib print_data.c include/print_data.h include/tables_dict.h
	gcc $(CFLAGS) $(INCLUDES) -c print_data.c -o lib/print_data.o 

lib/check_data.o: lib check_data.c include/check_data.h include/tables_dict.h
	gcc $(CFLAGS) $(INCLUDES) -c check_data.c -o lib/check_data.o

lib/tables_dict.o: lib tables_dict.c include/tables_dict.h include/table_defs.h mysql-source/include/my_config.h
	gcc $(CFLAGS) $(INCLUDES) -c tables_dict.c -o lib/tables_dict.o

# BINARIES

page_parser: page_parser.c lib/tables_dict.o lib/libut.a
	gcc $(CFLAGS) $(INCLUDES) -o $@ $^

page_printer: page_printer.c
	gcc $(CFLAGS) $(INCLUDES) -o $@ $<

constraints_parser: constraints_parser.c lib/tables_dict.o lib/print_data.o lib/check_data.o lib/libut.a lib/libmystrings.a
	gcc $(CFLAGS) $(INCLUDES) -o $@ $^

innochecksum_changer: innochecksum.c include/innochecksum.h
	gcc $(CFLAGS) $(INCLUDES) -o $@ $<

ibdconnect: ibdconnect.c include/ibdconnect.h include/sys_defs.h
	gcc $(CFLAGS) $(INCLUDES) -o $@ $<

bgrep: bgrep.c
	gcc $(CFLAGS) $(INCLUDES) -o $@ $<

s_tables: s_tables.c
	gcc $(CFLAGS) $(INCLUDES) -o $@ $<

s_indexes: s_indexes.c
	gcc $(CFLAGS) $(INCLUDES) -o $@ $<

ibd_rename_ids: ibd_rename_ids.c include/innochecksum.h
	gcc $(CFLAGS) $(INCLUDES) -o $@ $<

sys_parser: sys_parser.c
	@ if test -z "`which mysql_config`"; then echo "sys_parser needs mysql development package"; fi; 
	gcc -o $@ $<  `mysql_config --cflags` `mysql_config --libs`

index_chk: index_chk.c
	gcc -o $@ $<

dict_parsers: all
	@mkdir -p bin
	@ln -fs table_defs.h.SYS_TABLES include/table_defs.h
	@make clean all
	@cp constraints_parser bin/constraints_parser.SYS_TABLES
	@ln -fs table_defs.h.SYS_INDEXES include/table_defs.h
	@make clean all
	@cp constraints_parser bin/constraints_parser.SYS_INDEXES
	@ln -fs table_defs.h.SYS_COLUMNS include/table_defs.h
	@make clean all
	@cp constraints_parser bin/constraints_parser.SYS_COLUMNS
	@ln -fs table_defs.h.SYS_FIELDS include/table_defs.h
	@make clean all
	@cp constraints_parser bin/constraints_parser.SYS_FIELDS

CC ?= gcc
MY_IN_CFLAGS += -O3 -g
MY_IN_INC = $(shell mysql_config --include)
MY_IN_LIBS = $(shell mysql_config --libs)

myinsane: myinsane.c
	$(CC) $(MY_IN_CFLAGS) $(MY_IN_INC) $(MY_IN_LIBS) -o $@ $<

# DEPENDENCIES FROM MYSQL

mysql-source/config.h:
	cd mysql-source && CFLAGS=-g ./configure --without-docs --without-man --without-bench --without-extra-tools

mysql-source/include/my_config.h: mysql-source/config.h
	cd mysql-source/include && $(MAKE) my_config.h

lib/libut.a: mysql-source/include/my_config.h
	cd mysql-source/innobase/ut && $(MAKE) libut.a
	ln -fs ../mysql-source/innobase/ut/libut.a lib/libut.a

lib/libmystrings.a: mysql-source/include/my_config.h
	cd mysql-source/strings && $(MAKE) libmystrings.a
	ln -fs ../mysql-source/strings/libmystrings.a lib/libmystrings.a

# DIST / INSTALL

dist: $(BINARIES)
	mkdir -p innodb_recovery
	cp $(BINARIES) innodb_recovery/
	tar czf innodb_recovery.tar.gz innodb_recovery
	rm -rf innodb_recovery

install: $(BINARIES)
	cp $(BINARIES) $(PREFIX)
    
# CLEAN

clean: 
	rm -f $(BINARIES)
	rm -rf lib constraints_parser.dSYM page_parser.dSYM

distclean: clean
	rm -rf innodb_recovery
	rm -f innodb_recovery.tar.gz
	cd mysql-source && (test -f Makefile && $(MAKE) -i distclean) || true
	rm -rf mysql-source/Docs/Makefile mysql-source/man/Makefile mysql-source/sql-bench/Makefile mysql-source/autom4te.cache
