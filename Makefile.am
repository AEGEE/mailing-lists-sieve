LIBTOOL_DEPS = @LIBTOOL_DEPS@
AM_CPPFLAGS = -Isrc/ -DSYSCONFDIR='"${sysconfdir}"' ${glib_CFLAGS} ${liblistserv_CFLAGS} ${gsasl_CFLAGS}

AM_CFLAGS = -Wall -Wextra

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

info_TEXINFOS			= doc/mailing-lists-sieve.texinfo
doc_mailing_lists_sieve_TEXINFOS	= doc/fdl-1.3.texi
bin_PROGRAMS = src/mailing-lists-sieve

src_mailing_lists_sieve_SOURCES = src/mailing-lists-sieve.c src/mailing-lists-sieve.h src/generators.c src/uploaders.c
src_mailing_lists_sieve_LDFLAGS = ${glib_LIBS}
src_mailing_lists_sieve_LDADD = -dlpreopen src/generators/sample.la -dlpreopen src/generators/listserv.la -dlpreopen src/uploaders/timsieved_fs.la -dlpreopen src/uploaders/managesieve.la

pkglib_LTLIBRARIES = src/generators/sample.la src/generators/listserv.la src/uploaders/timsieved_fs.la src/uploaders/managesieve.la

src_generators_listserv_la_LIBADD = ${glib_LIBS} ${liblistserv_LIBS}
src_generators_listserv_la_LDFLAGS = -module -avoid-version -no-undefined -static
# src_generators_listserv_la_SOURCES = src/generators/listserv.c

src_generators_sample_la_LDFLAGS = -module -avoid-version -no-undefined -static

src_uploaders_managesieve_la_LIBADD = ${gsasl_LIBS}
src_uploaders_managesieve_la_LDFLAGS = -module -avoid-version -no-undefined -static

src_uploaders_timsieved_fs_la_LDFLAGS = -module -avoid-version -no-undefined -static

install-exec-hook:
	rm $(DESTDIR)$(pkglibdir)/*
	rmdir "$(DESTDIR)$(pkglibdir)"
