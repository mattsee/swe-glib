include $(top_srcdir)/swe-glib.mk

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = swe swe/src swe/doc src po data tests

if ENABLE_GTK_DOC
SUBDIRS += docs/reference/swe-glib
endif

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

intltool_extra = intltool-extract.in intltool-merge.in intltool-update.in

EXTRA_DIST += $(intltool_extra) $(header_DATA) autogen.sh

DISTCLEANFILES += intltool-extract intltool-merge intltool-update

MAINTAINERCLEANFILES += ChangeLog

distclean-local:
	if test "$(srcdir)" = "."; then :; else \
	    rm -f ChangeLog; \
	fi

ChangeLog:
	$(AM_V_GEN) if test -d "$(srcdir)/.git"; then \
	    (GIT_DIR=$(top_srcdir)/.git ./missing --run git log -M -C --name-status --date=short --no-color) | fmt --split-only > $@.tmp \
	    && mv -f $@.tmp $@ \
	    || ($(RM) $@.tmp; \
	        echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	        (test -f $@ || echo git log is required to generate this file >> $@)); \
	else \
	    test -f $@ || \
	    (echo A git checkout and git log is required to generate ChangeLog >&2 && \
	    echo A git checkout and git log is required to generate this file >> $@); \
	fi

.PHONY: ChangeLog

