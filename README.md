# p2padga

A small terminal BitTorrent client built on [libtorrent](https://github.com/arvidn/libtorrent) and [ncurses](https://invisible-island.net/ncurses/).

![p2padga_good](https://github.com/mm4rk3t/p2padga/assets/45017214/fdf4d055-cda6-4ff7-8639-406f95299c6a)

- Add magnet links, bare info-hashes or `.torrent` files, from the UI or the command line
- Pause, resume, recheck, reorder the queue, remove (with or without the downloaded files)
- Choose which files of a torrent to download
- Torrents and their progress survive restarts
- Log view for tracker and peer errors

## Installing

You need a C++20 compiler, `make`, `pkg-config`, and the libtorrent and ncurses development packages:

| Distro | Packages |
|---|---|
| Fedora | `gcc-c++ make pkgconf rb_libtorrent-devel ncurses-devel boost-devel` |
| Debian / Ubuntu | `g++ make pkg-config libtorrent-rasterbar-dev libncurses-dev libboost-dev` |
| Arch | `base-devel libtorrent-rasterbar ncurses boost` |

Then:

```sh
git clone https://github.com/mm4rk3t/p2padga
cd p2padga
make
sudo make install          # installs to /usr/local/bin
```

`make install PREFIX=~/.local` installs for your user only. `make uninstall` removes it again.

## Using it

```sh
p2padga                                   # open the client
p2padga file.torrent 'magnet:?xt=...'     # add torrents on startup
p2padga -s ~/Videos file.torrent          # ...into a specific folder
```

| Key | Action |
|---|---|
| `a` | add a magnet link, info-hash or `.torrent` file |
| `j` `k` / arrows | move; `g` `G` first / last |
| `enter` / `l` | files of the selected torrent (`space` toggles a file, `a` all, `n` none) |
| `p` / `space` | pause or resume |
| `+` `-` | move up / down in the download queue |
| `r` | recheck downloaded data |
| `d` | remove the torrent, keep its files |
| `D` | remove the torrent and delete its files |
| `L` | log |
| `?` | help |
| `q` | quit (progress is saved) |

## Configuration

The first run writes a commented config to `~/.config/p2padga/config`:

```ini
save_path = ~/Downloads
listen_interfaces = 0.0.0.0:6881,[::]:6881
outgoing_interface =        # e.g. tun0: send all traffic through your VPN
encryption = prefer         # prefer | require | disable
dht = true
lsd = true
upnp = false                # don't open router ports unless you ask for it
natpmp = false
anonymous_mode = false
download_limit = 0          # KiB/s, 0 = unlimited
upload_limit = 0
max_active_downloads = 3
max_active_seeds = 5
```

Torrent state lives in `~/.local/share/p2padga/` (it follows `XDG_CONFIG_HOME` and `XDG_DATA_HOME`).

## Security

Torrents come from strangers, so p2padga treats everything inside them as hostile:

- **Terminal output is escaped.** Torrent names, file names and tracker messages are decoded and stripped of control characters before they're drawn. A name can't inject escape sequences to recolour the screen, retitle your terminal or hide text with right-to-left overrides. Text is never used as a format string.
- **Input is bounded.** `.torrent` files over 32 MiB are refused before parsing, and text input has a length limit.
- **Paths can't escape.** libtorrent sanitizes file paths inside torrents, so `../../etc/passwd` lands inside the torrent's folder.
- **State is private.** Config, resume data and session state are written atomically with mode `0600` in `0700` directories, and named by info-hash only. A lock file stops two instances from corrupting each other's state.
- **Network hardening.** Peers and trackers can't make p2padga connect to privileged ports or local services, HTTPS tracker certificates are verified, internationalized tracker hostnames (homograph tricks) are refused, and UPnP/NAT-PMP are off by default.
- **Hardened build.** `_FORTIFY_SOURCE=3`, stack protector, full RELRO, PIE and non-executable stack.

What it does **not** do: BitTorrent is not anonymous. Peers and trackers see your IP address, and peer encryption only hides the traffic from simple throttling. If that matters to you, use a VPN and set `outgoing_interface`, so nothing leaks out if the VPN drops.

## License

[MIT](LICENSE)
