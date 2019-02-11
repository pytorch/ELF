rm CMakeCache.txt
module purge
module load anaconda3
source scripts/devmode_set_pythonpath.sh
#module load gcc/7.1.0
module load cuda/9.0
module load cudnn/v7.0-cuda.9.0
module load NCCL/2.3.7-1-cuda.9.0
source activate otgo12
source scripts/devmode_set_pythonpath.sh
conda install -y numpy zeromq pyzmq
conda install -y -c pytorch pytorch cuda90
find . -iname 'flags.make' -exec sed -i "s/std=c++14/std=c++17/g" {} \;
make -j
