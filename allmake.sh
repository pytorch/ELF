rm CMakeCache.txt
module load anaconda3
module load cuda/9.0
module load cudnn/v7.0-cuda.9.0
source activate otgo12
conda install numpy zeromq pyzmq
conda install -c pytorch pytorch cuda90
find . -iname 'flags.make' -exec sed -i "s/std=c++14/std=c++17/g" {} \;
make -j
