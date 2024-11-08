build: build_gimp build_cli

build_gimp: install_gimp

install_gimp:
	gimptool-2.0 --install ./src/file-ilbm.c

build_cli:
	/usr/bin/gcc -fdiagnostics-color=always -g -o ilbm_cli ./src/ilbm_cli.c

test_cli: build_cli
	./ilbm_cli examples/NEOLOGO.BRS_NEO_WhalesVoyage.ilbm
