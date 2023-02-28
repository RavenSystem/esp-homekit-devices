# -*- mode: ruby -*-
# vi: set ft=ruby :
BOX = "ubuntu"
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
    if BOX == "ubuntu"
        config.vm.box = "ubuntu/trusty64"
        config.vm.provision "shell", path: ".ubuntu-provisioner.sh"
    else
        config.vm.box = "moisesguimaraes/centos72-64"
        config.vm.provision "shell", path: ".centos-provisioner.sh"
    end
end
