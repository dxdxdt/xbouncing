# xbouncing
The X11 bouncing DVD logo. A little gimmick for a new video.

![Screen capture of xbouncing in action](doc/animated.webp)

## Cloud Demo
If you're running Linux, you can simply use SSH X11 forwarding to try it out
without installing anything at all. However, due to network delay, it won't be
as smooth as running it on your machine locally.

```
ssh -X dvd@sshs.snart.me
```

If you're running Mac or Windows, you may use **XQuartz** or **Xming**.

## INSTALL
To build it from source and run it on your machine yourself,

```sh
sudo dnf install git cmake gcc libX11-devel libXrandr-devel

git clone https://github.com/dxdxdt/xbouncing
cd xbouncing

cmake -B build
cd build

make
sudo make install
```

## Usage
```sh
xbouncing
```

Interrupt(`Ctrl` + `C`) or terminate.
