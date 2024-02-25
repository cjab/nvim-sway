nvim-sway
=========

`nvim-sway` allows navigation between both sway windows and neovim splits.
It does this using neovim and sway rpc. No neovim plugins are required.

After installation replace your focus bindings in your sway config
with `nvim-sway`.

```
bindsym $mod+$left exec nvim-sway left
bindsym $mod+$down exec nvim-sway down
bindsym $mod+$up exec nvim-sway up
bindsym $mod+$right exec nvim-sway right
```

## Details
Communication with sway is done over the socket found in the `$SWAYSOCK`
environment variable.

The neovim socket is found by first finding the pid of the target neovim
instance and then searching in the standard neovim socket locations.
These are `$XDG_RUNTIME_DIR` and `$TMPDIR/nvim.$USER`.

## Dependencies
* [cjson](https://github.com/DaveGamble/cJSON)
* [msgpack-c](https://github.com/msgpack/msgpack-c/tree/c_master)
* [sway](https://swaywm.org/)
* [neovim](https://neovim.io/)

## Installation

### Nix
```
nix shell github:cjab/nvim-sway:nvim-sway
```

### Source
```
make && make install
```

## Development

The easiest way to build this project is to use the dev environment provided
in the flake.nix.

Otherwise, ensure that the dependencies listed above are installed and then run:

```
make
```

## Alternatives
This was heavily inspired by [vim-sway-nav](https://git.sr.ht/~jcc/vim-sway-nav)
and if vim support is important to you it still may be the way to go!

The initial motivation for `nvim-sway` was the large amount of latency
found in [vim-sway-nav](https://git.sr.ht/~jcc/vim-sway-nav) when changing focus from sway windows with deep process trees.

Along the way, I also realized if the scope was limited to neovim that I could
avoid requiring a (neo)vim plugin altogether.
