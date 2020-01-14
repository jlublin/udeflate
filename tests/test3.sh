#!/bin/bash

diff <($TARGET $TESTS_DIR/test3/dot.deflate | grep 'Data:') \
     $TESTS_DIR/test3/dot.out \
     1>/dev/null
