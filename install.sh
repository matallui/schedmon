#!/bin/bash

cd driver; make; sudo ./smon_load; cd ..
cd tool; make; sudo ln -s bin/smon /usr/local/bin/smon; cd ..


