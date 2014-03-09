lib_files := $(shell find libraries/TLC5926 -type f)
arduino_dir := $(shell which arduino | xargs --no-run-if-empty realpath | xargs --no-run-if-empty dirname)
arduino_inc_dir := $(arduino_dir)/hardware

# a=`which arduino` && b=`realpath $$a` && echo `dirname $$b`/hardware

.PHONY : all
all : build_dir README.md arduino_TLC5926.zip

.PHONY : clean
clean : build_dir

.PHONY : lib_files
lib_files :
	@echo $(lib_files)

.PHONY : build_dir
build_dir :
	@mkdir -p build
	@rm -rf build/*

README.md : libraries/TLC5926/TLC5926.cpp
	awk 'FNR==2,/*\// {if ($$0 != "*/") {print}}' $< | sed 's/^    //' > $@
	echo >> $@
	awk '/\/* Use:/,/*\// {if ($$0 == "*/") {next}; sub(/^\/\*/, ""); print}' $< | sed 's/^ //' >> $@

arduino_TLC5926.zip : $(lib_files)
	rm $@ 2>/dev/null
	cd libraries && zip -r ../$@ TLC5926 --exclude TLC5926/.*

libraries/TLC5926/keywords.txt : libraries/TLC5926/TLC5926.h
ifndef arduino_dir
	@echo WARNING: Couldn\'t rebuild $@ because couldn\'t find arduino command
else ifeq ($(shell which gccxml),)
	@echo WARNING: Couldn\'t rebuild $@ because couldn\'t find gccxml command
else ifeq ($(shell which xsltproc),)
	@echo WARNING: Couldn\'t rebuild $@ because couldn\'t find xsltproc command
else
	echo "-D __COMPILING_AVR_LIBC__=1" > build/TLC5926.inc
	find $(arduino_inc_dir) -name '*.h' | xargs -n 1 dirname |  sort -u | awk '{print "-I"$$0}' >> build/TLC5926.inc
	ln -s -f `pwd`/$< build/TLC5926.hpp
	gccxml --gccxml-gcc-options build/TLC5926.inc build/TLC5926.hpp -fxml=build/TLC5926.h.xml
	xsltproc --stringparam help_page http://2splat.com/dandelion/reference/arduino-TLC5926.html --stringparam class_name TLC5926 make_keywords.xsl build/TLC5926.h.xml | sort -u > $@
endif
