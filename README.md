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

To install the compiled binary in `/usr/local/bin` (or wherever you want, the script can easily be modified), simply run:
```bash
sudo ./install.sh
```

Alternatively, uninstall with:
```bash
sudo ./uninstall.sh
```
Remember that the directory must match the one used in `install.sh`.

## Usage
If the compilation ended successfuly, there is going to be a folder named `bin` where the `p2padga` executable resides. Once you run the program, it should be clear how to use it. 


## Contributing
Pull requests are welcome. If there's anything you wish to discuss, open an issue or contact me using the info on my profile. Please inform me of any bugs you find, I hope you find the program useful even in this very early stage.

## Known bugs
- some strings are so long that they "overflow" the row

## TO-DO

I will separate what needs to be done in two categories so that I can tell at a glance what to prioritize.

### Important
#### Files
- [ ] progress per file
- [ ] choose not to download
- [ ] create object to organize things better

#### Torrent
- [ ] show list of trackers and seeders
- [ ] handle behaviour after completion
- [ ] resume previous sessions

#### Client
- [ ] add log/debug window
- [ ] error handling
- [ ] optimize windows refreshs, data fetchs, draws, etc
- [ ] filter
- [ ] sort

### L4L (Leave for Later)
- [ ] limit string length if it is grater than padding between fields
- [ ] adjust padding of different fields (for example: make "ID" 5 and "Name" 15)
- [ ] change row color based on status and whether the torrent is paused
- [ ] add ETA for completion
- [ ] implement config file


## License
[MIT](https://choosealicense.com/licenses/mit/)