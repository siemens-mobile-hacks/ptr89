#!/bin/bash
set -e
set -x
cd $(dirname $0)/../

fakeroot debian/rules binary
fakeroot debian/rules clean

ls -lah ../*.deb
