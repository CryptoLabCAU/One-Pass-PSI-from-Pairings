# Efficient One-Pass Private Set Intersection from Pairings with Offline Preprocessing
This project implements an efficient one-pass private set intersection from pairings with offline preprocessing

## Required Libraries
Before building the project, ensure the following libraries are installed:

1. **OpenSSL**
2. **GMP**
3. **Relic library**

## Build the Project

```bash
mkdir build
cd build
cmake ..
make
```

## Running the Code
- `-p`: Port number
- `-n`: Sender's dataset log size
- `-m`: Receiver's dataset log size

## Example
``` bash
./bin/Semi_Honest_sender -p 1234 -n 8 -m 8
./bin/Semi_Honest_receiver -p 1234 -n 8 -m 8
```
