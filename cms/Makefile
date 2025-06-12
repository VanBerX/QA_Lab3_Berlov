   # re: ``diagnostics-{color,show-caret}''
   # fuck the gcc team! only idiots can like these 'new' diags.
   # if you use older versions of gcc (e.g. 3.* or 4.*), build
   # with smth. like
   #                   CC=gcc CXX=g++ make your_target
   # (*YES*, gcc 3.* is perfectly okay to build this soft)
CC = gcc -ansi -fdiagnostics-color=never -fno-diagnostics-show-caret
CFLAGS = -Wall -ggdb

CXX = g++ -ansi -fdiagnostics-color=never -fno-diagnostics-show-caret
CXXFLAGS = -Wall -ggdb -I../lib -I../lib/inifile

AR = ar

STATIC =
#STATIC = -static
#STATIC = -static -s

LIBS = -lscriptpp -linifile -lstfilter -L../lib/scriptpp -L../lib/inifile\
	-L../lib/stfilter
LIBDEPS = ../lib/scriptpp/libscriptpp.a ../lib/inifile/libinifile.a\
	../lib/stfilter/libstfilter.a

CGILIBS = -lmd5 -lcaptcha -L../lib/md5 -L../lib/captcha
CGILIBDEPS = ../lib/md5/libmd5.a ../lib/captcha/libcaptcha.a

MAINBINARIES = thalassa thalcgi.cgi dullcgi.a
AUXBINARIES = imgsize_demo a.out

all:	$(MAINBINARIES)

%.o:	%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

THALASSA_MOD = thalassa.o database.o dbsubst.o basesubs.o \
	imgsize.o generate.o errlist.o dbforum.o forumgen.o \
	filters.o fpublish.o arrindex.o fileops.o urlenc.o \
	main_all.o main_gen.o main_lst.o main_upd.o

THALCGI_MOD = thalcgi.o tcgi_db.o tcgi_ses.o xcgi.o xcaptcha.o \
	tcgi_sub.o basesubs.o cgicmsub.o imgsize.o makeargv.o \
	invoke.o emailval.o memmail.o tcgi_rpl.o filters.o fileops.o \
	roles.o fnchecks.o qsrt.o urlenc.o binbuf.o xrandom.o

DULLCGI_MOD = dullcgi.o xcgi.o basesubs.o cgicmsub.o imgsize.o fnchecks.o \
	urlenc.o xrandom.o



thalassa:	$(THALASSA_MOD) $(LIBDEPS)
	$(CXX) $(STATIC) $(THALASSA_MOD) $(LIBS) -o $@

thalcgi.cgi: $(THALCGI_MOD) $(LIBDEPS) $(CGILIBDEPS)
	$(CXX) $(STATIC) $(THALCGI_MOD) $(LIBS) $(CGILIBS) -o $@

dullcgi.a: dullcgi.mri $(DULLCGI_MOD) $(LIBDEPS) $(CGILIBDEPS)
	$(AR) -M < $<

imgsize_demo: imgsize.c
	$(CC) $(STATIC) $(CFLAGS) -g -D IMGSIZE_DEMO_MAIN -o $@ $<

../lib/scriptpp/libscriptpp.a:
	cd ../lib/scriptpp/ ; $(MAKE)

../lib/inifile/libinifile.a:
	cd ../lib/inifile/ ; $(MAKE)

../lib/stfilter/libstfilter.a:
	cd ../lib/stfilter/ ; $(MAKE)

../lib/captcha/libcaptcha.a:
	cd ../lib/captcha ; $(MAKE)

../lib/md5/libmd5.a:
	cd ../lib/md5 ; $(MAKE)

deps.mk:	$(wildcard *.cpp) _version.h
	$(CXX) -MM  $(wildcard *.cpp) > $@

_version.h: Version
	echo '#define THALASSA_VERSION "'`head -1 Version`'"' > $@
	echo '#define THALASSA_VERSIONID '`tail -1 Version` >> $@

clean:
	rm -rf *.o *~ $(MAINBINARIES) $(AUXBINARIES) deps.mk _version.h

ifneq (clean, $(MAKECMDGOALS))
-include deps.mk
endif
