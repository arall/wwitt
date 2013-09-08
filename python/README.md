
Linux config
------------

Important! You need to add more filedescriptors to your user in order to run the port scanner. To do so:
1. Edit /etc/security/limits.conf
2. Add line "user soft nofile 4096" where user is your username (root?)
3. Add line "user hard nofile 10240" where user is your username
4. Logout and login in the machine (if you use SSH, get out and connect again)


