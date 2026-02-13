# The Shit
___
*The Shit* is a better version of *The Fuck*, written in C++
*The Fuck* is a magnificent app, inspired by a @liamosaur tweet, that corrects errors in previous console commands.

*The Shit* comes complete with Levenshtein autocorrect (Yaknow, that shit nobody knows how to spell so they use autocorrect to spell it for them)

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

Then you can either add *The Shit* to your path, alias, or just copy it to your /usr/local/bin (Note: if you add it as an alias, make sure it's named "shit" otherwise *The Shit* won't be able to properly detect the last command)
```bash
sudo cp shit /usr/local/bin/shit
```

<img width="369" height="385" alt="image" src="https://github.com/user-attachments/assets/9f99ec9f-b6a7-4e4a-871c-75e46812eaa8" />
