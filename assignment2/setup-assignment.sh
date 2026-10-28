#!/bin/bash

ASSIGNMENT_DIR=./assignments-fall26/assignment2
DO_ARM=false

while [[ $# -gt 0 ]]; do
  case $1 in
    --arm)
      DO_ARM=true
      shift
      ;;
    *)
      shift # ignore
      ;;
  esac
done


pushd /home/ubuntu/

#Make sure scripts are executable
chmod u+x $ASSIGNMENT_DIR/*.sh
chmod u+x $ASSIGNMENT_DIR/src/*.sh
chmod u+x $ASSIGNMENT_DIR/src/sr_solution*

#configure and install dependencies
if $DO_ARM; then
    sudo $ASSIGNMENT_DIR/configure-arm.sh
else
    sudo $ASSIGNMENT_DIR/configure.sh
fi
sudo $ASSIGNMENT_DIR/install.sh

popd
