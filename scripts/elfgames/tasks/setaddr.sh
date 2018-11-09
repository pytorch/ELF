sed "s/XXX.XXX.XXX.XXX/`hostname -I | sed 's/ .*//g'`/g" server_addrs.py.origin > server_addrs.py
