#!/bin/bash

diff <($TARGET $TESTS_DIR/test2/gradient.deflate | grep 'Data:') \
     $TESTS_DIR/test2/gradient.out \
     1>/dev/null
