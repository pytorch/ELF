Simple usage
===========

```
mkdir build
cd build
cmake ..
make
```

Then `source [ELF repo]/scripts/devmode_set_pythonpath.sh`.

Then run `python test_elf_python.py --batchsize 8 --num_game_thread 16`. Note that `num_game_thread` should be larger than `batchsize`, otherwise the program will hang. 

Note that there is a memory issue when exiting the program (should not change the results). We will solve it soon. 

