# Build Your Own BitTorrent

[![progress-banner](https://backend.codecrafters.io/progress/bittorrent/7db922cc-41f3-4282-9d55-9d6433f29439)](https://app.codecrafters.io/users/codecrafters-bot?r=2qF)

Welcome to the "Build Your Own BitTorrent" challenge! This repository serves as a starting point for C++ solutions to the challenge.

## Overview

This codebase provides functionalities for decoding bencoded values (strings, integers, lists, and dictionaries), parsing .torrent files, and downloading either a single piece or the entire file using the BitTorrent protocol.

## Getting Started

To begin, ensure that you have `cmake` installed locally on your system. Then, execute the `./your_bittorrent.sh` script to run your program, which is implemented in `src/Main.cpp`.

## Usage

The provided `your_bittorrent.sh` script supports various functionalities:

1. **Decoding Bencoded Values**: Use `./your_bittorrent.sh decode bencoded_value` to decode bencoded values.

2. **Parsing Torrent Files**: Run `./your_bittorrent.sh info sample.torrent` to parse a .torrent file.

3. **Discovering Peers**: Send a GET request to an HTTP tracker to discover peers for file download with `./your_bittorrent.sh peers sample.torrent`.

4. **Establishing TCP Connections**: Initiate a TCP connection with a peer and complete a handshake using `./your_bittorrent.sh handshake sample.torrent <peer_ip>:<peer_port>`.

5. **Downloading Single Piece of File**: Utilize `./your_bittorrent.sh download_piece -o where_to_download sample.torrent number_of_piece` to download a single piece of the file.

6. **Downloading Entire File**: Download the entire file using `./your_bittorrent.sh download -o where_to_download sample.torrent`.
