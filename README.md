# System event monitor

Project for creating a system, which's events are monitored for changes in watched files and directories. 

Purpose of this project was to practive system's programming, code a small Daemon for logging desired System events, and send the logged events to another host with SSH.

![Setup Diagram](./pictures/setup_diagram.jpg)

Deamon (watcher) runs on the backround on the VM, monitoring the set directory/file. After monitoring is stopped with a shutdown signal, the program connects to the host machine via SSH, and sends the log file file to desired host machine directory.

## Usage

SSH connection from the VM to the host machine needs to established first. SSH needs to enabled for host and the VM.

Check SSH status with
``` bash
sudo systemctl status ssh
```

If SSH is disabled
``` bash
sudo systemctl enable ssh
```
``` bash
sudo systemctl start ssh
```

A keypair needs to be created so that VM can connect without the password.
Example
``` bash
ssh-keygen -b 2048 -t rsa
```
Use of best practices when creating and using SSH keys is adviced.

``` bash
make
```

``` bash
./watcher [/path/to/file]
```
