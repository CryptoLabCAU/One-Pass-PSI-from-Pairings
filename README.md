# Efficient One-Round Private Set Intersection using Pairings with Offline Preprocessing

This project implements an efficient one-round private set intersection using pairings with offline preprocessing

## Required Libraries
Before building the project, ensure the following libraries are installed:

1. **OpenSSL**
2. **GMP** (GNU Multiple Precision Arithmetic Library)
3. **Relic** (Cryptographic toolkit)

## Build the Project

```bash
mkdir build
cd build
cmake ..
make
```

## Running the Code
- `-p`: Port number
- `-t`: Number of threads
- `-n`: Sender's dataset log size
- `-m`: Receiver's dataset log size

## Example
``` bash
./Semi_Honest_sender -p 1234 -n 8 -m 8
./Semi_Honest_receiver -p 1234 -n 8 -m 8
```
