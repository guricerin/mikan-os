#!/bin/bash
set -eux

cd $HOME
git clone https://github.com/uchan-nos/mikanos-build.git osbook
sudo apt install ansible
cd $HOME/osbook/devenv
ansible-playbook -K -i ansible_inventory ansible_provision.yml