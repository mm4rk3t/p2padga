# p2padga

p2padga is a simple bittorrent client built using [libtorrent](https://github.com/arvidn/libtorrent) and [ncurses](https://github.com/mirror/ncurses).

**Disclaimer:** As of now, there are plenty of unimplemented features and the code might be full of bugs/bad practices. Thread with care, and by no means use this to learn. This is my first relatively "big" project, which I am using to learn how to combine libraries, build systems, source control systems, et cetera. So don't be surprised if it sucks.

## Installation

Before starting, make sure you have the following dependences installed on your system:

- `boost`
- `ncurses`
- `libtorrent`
 
Clone the repository.

```bash
git clone https://github.com/mm4rk3t/p2padga
```
Run `make` inside of it to begin compilation.

```bash
cd p2padga && make
```

## Usage
If the compilation ended successfuly, there is going to be a folder named `bin` where the `p2padga` executable resides. Once you run the program, it should be clear how to use it. 


## Contributing
Pull requests are welcome. If there's anything you wish to discuss, open an issue or contact me using the info on my profile. Please inform me of any bugs you find, I hope you find the program useful even in this very early stage.

## Known bugs
- fetching metadata seems stalled when adding magnet links, however it is not, give it time
- messed up strings
- some strings are so long that they "overflow" the row
- ghost handles (visible when viewing info after being deleted)

## TO-DO
- [ ] limit string length if it is grater than padding between fields
- [ ] show list of trackers, seeders and progress per file
- [ ] add log/debug window (use libtorrent alerts if possible)
- [ ] adjust padding of different fields (for example: make "ID" 5 and "Name" 15)
- [ ] choose not to download some files
- [ ] change torrent priority
- [ ] create File object to organize things better (similar to Torrent object)
- [ ] change row color based on status and whether the torrent is paused
- [ ] error handling
- [ ] add ETA

## License
[MIT](https://choosealicense.com/licenses/mit/)