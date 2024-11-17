# audio-man
A simplistic audio manager for playback and recording.

## Quick test
Generate build files
```shell
cmake -S . -B build
```

Build the test console app
```shell
cmake --build build/ --clean-first -j
```

Run the test console app 
```shell
./build/audio_man
```
This will run a console test app which records from the mic for 10 seconds and replays the audio back.  
Additionally you can pass a file path to a .wav file for playback 2 times overlapping each other, this tests sudden clip cancellation.

## Credits
* [miniaudio](https://github.com/mackron/miniaudio)
* [miniz](https://github.com/richgel999/miniz)
