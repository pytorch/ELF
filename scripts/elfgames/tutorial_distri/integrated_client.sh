ADDR="[`hostname -I | cut -d ' ' -f 1`]"
echo $ADDR

game=elfgames.tutorial_distri.client_loader model=simple model_file=elfgames.tutorial_distri.model python3 -u client.py \
    --base.batchsize 16 --base.num_game_thread 64 --net.port 2341 --net.server_addr $ADDR --keys_in_reply V "$@"
