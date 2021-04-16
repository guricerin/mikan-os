#!/bin/bash
set -eux

cd $HOME
git clone https://github.com/uchan-nos/mikanos-build.git osbook
sudo apt install ansible
cd $HOME/osbook/devenv
ansible-playbook -K -i ansible_inventory ansible_provision.yml

readonly SCRIPT_ROOT=$(cd $(dirname ${0}); pwd)
cd $HOME/edk2
ln -s $SCRIPT_ROOT/MikanLoaderPkg ./