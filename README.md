# The Shit
___
*The Shit* is a better version of *The Fuck*, written in C++
Complete with Levenshtein autocorrect (Yaknow, that shit nobody knows how to spell so they use autocorrect to spell it for them)

Also, *The Shit* is INSTANT, unlike *The Fuck*, where it can take multiple seconds for it to come up with a result

*The Shit* was tested using ZSH. It is supposed to support Bash, but Bash is untested, please let me know if there are any issues with bash using *The Shit*
___
# Building

Either use cmake as normal
```bash
mkdir build && cd build
cmake ..
make
```
or you can use G++
```bash
g++ -std=c++20 -o shit main.cpp
```

Then you can either add *The Shit* to your path, alias, or just copy it to your /usr/local/bin
```bash
sudo cp shit /usr/local/bin/shit
```
