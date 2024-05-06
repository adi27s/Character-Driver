# Character-Driver
Test Char Driver on BBB


git init <br>
git submodule add -b 5.10-rt https://github.com/beagleboard/linux.git ldd/source/linux/ <br>
git submodule update --init --recursive <br>
git add . <br>
git commit -m "Inital commit with Linux submodule" <br>

git clone <url> <br>
cd ldd <br>
git submodule update --init --recursive <br>
