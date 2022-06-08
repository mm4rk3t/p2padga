a simple bittorrent client that uses libtorrent and ncurses


### TODO / bugs to fix
* IMPORTANT

* EASY
- [ ] limit str size and add ...
- [ ] clear lines to avoid things like "finisheding"

* OTHER
- [ ] show list of trackers, seeders and progress per file
- [ ] add log window
- [ ] add alerts
- [ ] adjust size of fields (for example: make ID 5 and Name 15)
- [ ] adjust magnitude of size and velocity (not only GB but KB, MB, B and TB too)
- [ ] choose not to download some files
- [ ] change priority
- [ ] create File object (similar to Torrent object)
- [ ] change color based on status and whether the torrent is paused
  
* DONE:
- [x] finish ui skeleton
- [x] upload to gh
- [x] add torrent backend
- [x] fix bug when adding magnet files
- [x] weird output @ the beginning
- [x] decouple output string from values
- [x] program state logic
- [x] can't output more than a single torrent at the same time  (can download more just fine)
- [x] check if speed is correct
- [x] select save location
- [x] add file viewer
- [x] Solve files bug when adding magnet links
- [x] add more torrent functionality

