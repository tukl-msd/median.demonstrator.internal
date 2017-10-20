#!/bin/bash

if ls /etc/init.d/system 1> /dev/null 2>&1; then
    echo "system already exists in /etc/init.d/"
else

    ln -s /home/median.demonstrator.internal/service/system /etc/init.d/system

    update-rc.d system defaults

fi