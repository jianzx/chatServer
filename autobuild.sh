#!/bin/bash

set -x

rm -rf "$PWD"/build/*
cd "$PWD"/build &&
	cmake .. &&
	make
