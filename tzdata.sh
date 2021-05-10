#!/bin/bash
export DEBIAN_FRONTEND=noninteractive

ln -fs /usr/share/zoneinfo/Europe/Warsaw /etc/localtime
apt-get update
apt-get install -y tzdata
dpkg-reconfigure --frontend noninteractive tzdata