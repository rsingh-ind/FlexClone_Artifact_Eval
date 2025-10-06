#!/bin/bash

rm WebServerVM.copy1
rm WebServerVM.frnd1

rm WebServerVM.copy2
rm WebServerVM.frnd2

./setxattr_generic -c WebServerVM.copy1 -p WebServerVM.raw -f WebServerVM.frnd1
./setxattr_generic -c WebServerVM.copy2 -p WebServerVM.raw -f WebServerVM.frnd2

