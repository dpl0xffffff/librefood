#!/bin/bash

if [ ! -d .venv ]; then
	sudo apk add --no-cache python3-dev py3-pip build-base gfortran openblas-dev pkgconfig
	python -m venv .venv 
	source .venv/bin/activate
	python -m pip install --upgrade pip setuptools wheel opencv-python pyzbar
fi

gcc *.c -o librefood $(pkg-config --cflags --libs gtk4 libsoup-3.0 json-glib-1.0) -lm
