kentSrc = ../../..
include ../../../inc/common.mk

O = msort.o

runtime.a: $(O) 
	ar rcus runtime.a $(O)

clean::
	rm -f ${O} runtime.a
