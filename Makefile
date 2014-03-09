lib_files=$(shell find libraries/TLC5926 -type f)

.PHONY : all
all : arduino_TLC5926.zip

.PHONY : lib_files
lib_files :
	@echo $(lib_files)

arduino_TLC5926.zip : $(lib_files)
	cd libraries && zip ../$@ `find TLC5926 -type f` 
