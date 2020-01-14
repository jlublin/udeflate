#!/bin/bash

diff <($TARGET $TESTS_DIR/test1/white.deflate | grep 'Data:') \
     $TESTS_DIR/test1/white.out \
     1>/dev/null
