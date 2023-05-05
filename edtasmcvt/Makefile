include defines.mk product.mk

CPPFLAGS = '-DVERSION="$(VERSION)"'
CFLAGS = -Wall -Werror -Wfatal-errors -O

ifndef top_dir
top_dir		:= $(PWD)
endif
src_dir		?= $(top_dir)
inc_dir		?= $(top_dir)
build_dir       ?= build

vpath %.c   $(src_dir)
vpath %.h   $(inc_dir)
vpath %.EXE $(top_dir)/cwsdpmi/bin
vpath %     $(top_dir)

prod_target	 = $(PRODUCT)
prod_obj_targets = $(PRODUCT).o
targets		 = $(prod_target)

tar_files	 = LICENSE README.md $(targets) $(tar_extras)

build_make = $(MAKE) \
		-C '$(build_dir)' \
		-I '$(top_dir)' \
		-f '$(top_dir)/Makefile'

clean_files     = $(targets)
clobber_files   = $(clean_files) $(build_dir)
distclean_files = $(clobber_files) build.*


all: FORCE | $(build_dir)
	$(build_make) $(targets)

release: all
	$(build_make) '$(TARBALLGZ)'

clean clobber distclean:
	$(call scrub_files_call,$($@_files))
	[ ! -d '$(build_dir)' ] || $(build_make) '$@'

$(build_dir):
	mkdir -p -- '$@'

$(prod_target): $(prod_obj_targets)
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o '$@'

$(TARBALLGZ): $(tar_files)
	tar -czP \
		--transform='s:^$(top_dir)/::' \
		--transform='s:^cwsdpmi/bin/CWSDPMI.EXE:cwsdpmi.exe:' \
		-f '$@' $^

show_package:
	@echo $(PRODUCT)

show_version:
	@echo $(VERSION)


.PHONY: all release clean clobber distclean show_package show_version FORCE
.DELETE_ON_ERROR:
