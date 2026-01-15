set -e

cd build

cmake ..

cmake --build . --parallel

cd ./KICKSTART_artefacts/Standalone/KICKSTART.app/Contents/MacOS

./KICKSTART