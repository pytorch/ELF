Simple Usage  
==============

First compile ELF: 
```
cd [ELF repo]
make elf
```

Open one terminal as the server, run 
```
. . ../../devmode_set_pythonpath.sh  # To setup path. 
python server.py --port 5566 --batchsize 2
```

Open two terminals as the clients, run
```
. . ../../devmode_set_pythonpath.sh  # To setup path. 
python client.py --port 5566
```

Note port can be any port number (except 0). 
