#!/bin/bash

cd driver; ./smon_unload; make clean; cd ..
cd tool; make clean; rm -rf /usr/local/bin/smon; cd ..


