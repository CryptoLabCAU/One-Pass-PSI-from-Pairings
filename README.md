# Efficient One-Pass Private Set Intersection from Pairings with Offline Preprocessing

This project implements the protocols described in [Efficient One-Pass Private Set Intersection from Pairings with Offline Preprocessing](https://link.springer.com/chapter/10.1007/978-3-032-07891-9_3).

## Build

Tested on Ubuntu 20.04.

```
sudo apt install build-essential cmake git perl libgmp-dev
git clone https://github.com/CryptoLabCAU/One-Pass-PSI-from-Pairings.git
cd One-Pass-PSI-from-Pairings
./build.sh                  # builds RELIC and OpenSSL into out/install
mkdir build && cd build
cmake ..
make
```

## Run

```
./frontend -u -list               # list tests
./frontend -u                     # run all tests
./frontend -u 0 -nn 12 -mm 12     # run test 0: semi-honest One-Pass PSI
./frontend -u 1 -nn 12 -mm 12     # run test 1: malicious One-Pass PSI
```

| Parameter | Description |
|---|---|
| `-nn`, `-mm` | log2 of the sender's / receiver's set size (default 10) |
| `-nt` | number of threads (default 1) |
| `-online` | use loopback TCP instead of in-memory channels |
| `-v` | print detailed timers |

Party times exclude time spent waiting for the other party.
