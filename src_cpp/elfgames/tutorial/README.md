Simple usage
===========

First compile by `make` in the ELF root folder (`$ELFROOT`). 

Then `source ./scripts/devmode_set_pythonpath.sh` in `$ELFROOT`.

Then go to `src_cpp/elfgames/tutorial` and run `python test_elf_python.py --batchsize 8 --num_game_thread 16`. Note that `num_game_thread` should be larger than `batchsize`, otherwise the program will hang. 

Note that there is a memory issue when exiting the program (should not change the results). We will solve it soon. 

