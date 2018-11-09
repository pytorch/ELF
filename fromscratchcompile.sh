source scripts/devmode_set_pythonpath.sh
rm CMakeCache.txt
module load anaconda3
module load gcc/7.1.0
source scripts/devmode_set_pythonpath.sh
conda create --name otgo13 --clone /private/home/ssengupta/.conda/envs/go10
source activate otgo13
conda install numpy zeromq pyzmq
conda install -c pytorch pytorch cuda90
source scripts/devmode_set_pythonpath.sh
#git clone https://github.com/pytorch/ELF.git . --recursive
git submodule sync && git submodule update --init --recursive
source scripts/devmode_set_pythonpath.sh
make
make test
