#!/bin/bash

gcc *.c -o librefood $(pkg-config --cflags --libs gtk4 libsoup-3.0 json-glib-1.0) -lm
