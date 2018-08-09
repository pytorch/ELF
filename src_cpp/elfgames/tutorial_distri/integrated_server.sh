ADDR="[`hostname -I | cut -d ' ' -f 1`]"
echo $ADDR

game=elfgames.tutorial_distri.server_loader model=simple model_file=elfgames.tutorial_distri.model python3 -u server.py --net.server_addr $ADDR --base.batchsize 128 --base.num_game_thread 16 \
    --num_episode 1000000 --num_minibatch 50 --tqdm --net.port 2341
