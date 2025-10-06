#!/bin/bash
ls *.eps | while read line;
do
epstopdf $line
done
