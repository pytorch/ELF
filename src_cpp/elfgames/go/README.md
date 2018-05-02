DarkForest Go Engine in ELF
===================
Here is an reimplementation of DarkForest Go Engine using ELF platform.

See [here](https://github.com/facebookresearch/darkforestGo) for the orignal LUA+Torch version. 

Training  
==========
```
sh ./train_df.sh --gpu [your gpu] --no_leaky_relu --list_file [your list of .sgf files]
```
Training with a single GPU. On GoGoD, Top 1 is around 51.2% after 8 days. Test performance is around 1% lower. 
```
1214:loss0[5000]: avg: 1.71844, min: 1.22093[1926], max: 2.31351[1187]
1214:loss1[5000]: avg: 2.83697, min: 2.35151[1591], max: 3.38029[131]
1214:loss2[5000]: avg: 3.43470, min: 2.91681[4142], max: 3.88072[2951]
1214:top1_acc[5000]: avg: 51.18156, min: 35.15625[2255], max: 67.96875[1369]
1214:top5_acc[5000]: avg: 82.61469, min: 69.53125[4479], max: 92.96875[1461]
1214:total_loss[5000]: avg: 4.28183, min: 3.55989[1591], max: 5.15988[1187]
```
Test  
=========
Run the same command but without backpropagation.
```
sh ./train_df.sh --gpu [your gpu] --load [your model] --multipred_no_backprop
```

Interactive console   
======================
You can play against the trained model. Rank is not established. 

```
sh ./console_df.sh --load [your model] --gpu [your gpu id] --data_aug 0
```

If you want to check training result interactively, use:
```
sh ./console_df_check_train.sh --load [your model] --gpu [your gpu id] --list_file [a file contains .sgf file list] --data_aug 0 --verbose
```
Here is a sample output:
```
Loaded file /home/yuandong/local/go/go_gogod/./Database/2012/2012-11-17n.sgf
#Handi = 0
[curr_game=63217][filename=./Database/2012/2012-11-17n.sgf] 0/184: qd, R4 (101)

  PreMove: 77
[curr_game=63217][filename=./Database/2012/2012-11-17n.sgf] 77/184: ql, R12 (269)

   A B C D E F G H J K L M N O P Q R S T
19 . O . . . . . . . . . . . . . . . . . 19
18 . X O O X X . . X . . . . . . . . . . 18
17 . O X X X X . O . X . O . O O O O O . 17
16 . . O X X O . . X + . O X X X X O X . 16
15 . O . O X O . . . X . . . . . . X . . 15
14 . O O O X O X . . . . O . . . . . . . 14
13 . X X O O X . . . . . . X). . . . . . 13
12 . X . X . . . . . . . . . . . . . . . 12
11 . . X O O . . . . . . . . O . O . X . 11     WHITE (O) has captured 1 stones
10 . . . + . . . . . + . . . . . + . . . 10     BLACK (X) has captured 2 stones
 9 . X O . . . . . . . . . . . . . X . . 9
 8 . . X O . . . . . . . . . . . . . . . 8
 7 . . X . . . . . . . . . . . . . . . . 7
 6 . . X O . . . . . . . . . . . . . . . 6
 5 . . X . . . . . . . . . . . . X . . . 5
 4 . . . + O . . . . + . . . . . + X . . 4
 3 . . . O . . . . . . O . . . O . . . . 3
 2 . . . . . . . . . . . . . . . . . . . 2
 1 . . . . . . . . . . . . . . . . . . . 1
   A B C D E F G H J K L M N O P Q R S T

DF Train>
```

For details of console commands, please check `df_console.py`.

MCTS & Selfplay
=============

Already incorporated. 

Reproduce AlphaGo Zero using ELF  
=================================  

This branch is from commit 1f10348af317821346df94dbf7cce38e4077ecc9 (facebookresearch/ELF)


