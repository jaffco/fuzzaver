set -e

cd build

cmake ..

cmake --build . --parallel

cd ./FUZZAVER_artefacts/Standalone/FUZZAVER.app/Contents/MacOS

./FUZZAVER