ACLOCAL_AMFLAGS = -I aclocal
ACLOCAL_FILES = aclocal/version.m4 aclocal/ax_create_stdint_h.m4
AM_CFLAGS = -Wall

bin_PROGRAMS = prxtool

TINYXML = $(srcdir)/tinyxml
INLCUDES = -I $(srcdir) -I $(TINYXML)

prxtool_SOURCES = \
	main.cpp \
	ProcessElf.cpp \
	ProcessPrx.cpp \
	NidMgr.cpp \
	VirtualMem.cpp \
	output.cpp \
	SerializePrx.cpp \
	SerializePrxToIdc.cpp \
	SerializePrxToXml.cpp \
	SerializePrxToMap.cpp \
	pspkerror.cpp \
	disasm.cpp \
	getargs.cpp \
	$(TINYXML)/tinyxml.cpp \
	$(TINYXML)/tinyxmlparser.cpp \
	$(TINYXML)/tinystr.cpp \
	$(TINYXML)/tinyxmlerror.cpp

noinst_HEADERS = \
	types.h \
	elftypes.h \
	prxtypes.h \
	output.h \
	NidMgr.h \
	ProcessElf.h \
	ProcessPrx.h \
	SerializePrx.h \
	SerializePrxToIdc.h \
	SerializePrxToXml.h \
	SerializePrxToMap.h \
	VirtualMem.h \
	pspkerror.h \
	disasm.h \
	getargs.h \
	$(TINYXML)/tinystr.h \
	$(TINYXML)/tinyxml.h

EXTRA_DIST = \
	$(ACLOCAL_FILES) \
	LICENSE \
	$(TINYXML)/VERSION \
	$(TINYXML)/changes.txt \
	$(TINYXML)/readme.txt

DISTCLEANFILES = _stdint.h
