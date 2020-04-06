# Build, test and pack scripts

# Description
 - `build.sh` - build script, details can be found in `./build.sh --help`
 - `test.sh` - script finds and run example contracts unit tests.
 - `pack.sh` - script packs output `.abi` and `.wasm` files to `.tar.gz` archive 

# Run
## On host machine
To run scripts directly on host machine just run particular scripts(e.g `./build.sh`, `./test.sh`).

## In docker
To run in docker just run scripts execution via `run` proxy(e.g `run build`, `run test`).
