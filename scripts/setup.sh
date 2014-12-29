#!/bin/sh

apt-get update

apt-get -y upgrade

apt-get -y install python

update-rc.d -f apache2 remove
