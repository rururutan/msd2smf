# MSD2SMF

`msd2smf` is a Python script that converts proprietary MSD-format MIDI files—used in some F&C Windows games—into standard MIDI files (Format 0).

This tool is a Python reimplementation of the original [msd2smf.rb](https://kurohane.net/silverhirame/soft_mus/index.html) script by SilverHirame.

## Features

- Converts all `.msd` files in the specified path to `.mid` (Format 0).
- Embeds loop information using FF7 PC style Meta events.
- Fully open-source and cross-platform (requires Python 3.6+).

## Usage

```bash
python msd2smf.py [path]
```

- [path] should be the directory containing .msd files.
- All .msd files in the directory will be automatically converted.
- Output files will be saved with the same filename but with a .mid extension in the same directory.

## Requirements

Python 3.6 or higher

## License

This project is licensed under the MIT License.

## Extra

The C language implementation is in the c_impl folder.

