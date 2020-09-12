#!/usr/bin/env python
import argparse
import pathlib
import sys
import textwrap
import time
from typing import Callable, Iterator

import serial
import tqdm

__version__ = '1.0.0'
BAUD_RATE = 500000
PAGE_SIZE = 128
ROM_SIZE = 65536
TIMEOUT = 5.0


def main():
    parser = argparse.ArgumentParser(
        description='GLS29EE512 programmer',
        epilog=textwrap.dedent("""\
            examples:
                dump the EEPROM contents to output.bin:
                    %(prog)s dump output.bin

                verify the EEPROM contents match input.bin:
                    %(prog)s verify input.bin

                write input.bin to the EEPROM:
                    %(prog)s write input.bin"""),
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-v',
                        '--version',
                        action='version',
                        version=f'gls29ee512 programmer {__version__}')
    parser.add_argument('-d',
                        '--device',
                        help='serial device to open (default /dev/ttyACM*)')
    parser.add_argument('command',
                        choices=['dump', 'verify', 'write'],
                        help='programmer operation, see examples below')
    parser.add_argument(
        'file',
        type=pathlib.Path,
        help='output file for dump mode / input file for verify and write'
        'mode')
    args = parser.parse_args()

    if args.device is None:
        args.device = find_serial_device()

    input_data = None
    if args.command in ('verify', 'write'):
        with open(args.file, 'rb') as f:
            input_data = f.read(ROM_SIZE)
            more = f.read(1)
            if len(input_data) != ROM_SIZE or len(more) != 0:
                print(f'Expected {ROM_SIZE} bytes in {args.file}',
                      file=sys.stderr)
                sys.exit(1)

    output_file = None
    if args.command == 'dump':
        if args.file.exists():
            print(f'Cowardly refusing to overwrite {args.file}',
                  file=sys.stderr)
            sys.exit(1)
        output_file = open(args.file, 'wb')

    with ExpectSerial(port=args.device) as dev:
        try:
            dev.expect(b'>')
        except TimeoutError as e:
            print(f'{e}\ncontinuing anyway...', file=sys.stderr)

        if args.command == 'write':
            for offset in page_iterator(dev,
                                        command='write',
                                        description='Writing',
                                        verify=lambda result: b'OK' in result):
                dev.write(input_data[offset:offset + PAGE_SIZE])

        if args.command in ('verify', 'write'):
            for offset in page_iterator(dev,
                                        command='verify',
                                        description='Verifying',
                                        verify=lambda result: b'OK' in result):
                dev.write(input_data[offset:offset + PAGE_SIZE])

        if args.command == 'dump':
            for _ in page_iterator(dev, command='read', description='Reading'):
                page = dev.read(PAGE_SIZE)
                if len(page) < PAGE_SIZE:
                    print(f'Timed out reading page {page}', file=sys.stderr)
                    sys.exit(1)
                output_file.write(page)


def page_iterator(device: 'ExpectSerial',
                  command: str,
                  description: str,
                  verify: Callable[[bytes], bool] = None) -> Iterator[int]:
    with tqdm.tqdm(unit='b',
                   unit_scale=True,
                   unit_divisor=1024,
                   total=ROM_SIZE) as t:
        t.set_description(description)
        for offset in range(0, ROM_SIZE, PAGE_SIZE):
            page = offset // PAGE_SIZE
            device.write(f'{command} {page}\n'.encode('ascii'))
            device.expect(b'#')
            yield offset
            result = device.expect(b'>')
            t.update(PAGE_SIZE)
            if verify and not verify(result):
                t.close()
                print(f'failed {command} at page {page}:', file=sys.stderr)
                print(result.replace(b'ready>', b'').decode('cp1252'),
                      file=sys.stderr)
                sys.exit(1)


class ExpectSerial(serial.Serial):
    def __init__(self, *args, **kwargs):
        kwargs.setdefault('baudrate', BAUD_RATE)
        kwargs.setdefault('timeout', TIMEOUT)
        super().__init__(*args, **kwargs)

    def expect(self, *characters: bytes):
        start = time.monotonic()
        buf = b''
        while time.monotonic() - start < TIMEOUT:
            buf += self.read()
            for char in characters:
                if buf.endswith(char):
                    return buf
        raise TimeoutError(f'buffer contents:\n{buf.decode("cp1252")}')


def find_serial_device():
    choices = list(pathlib.Path('/dev').glob('ttyACM*'))
    if len(choices) == 0:
        print('No devices found in /dev/ttyACM*', file=sys.stderr)
        sys.exit(1)
    if len(choices) > 1:
        print('More than one device found in /dev/ttyACM*', file=sys.stderr)
        sys.exit(1)
    return str(choices[0])


if __name__ == '__main__':
    main()
