#!/usr/bin/env python
import argparse
import time

import sys

import serial
import tqdm

BAUD_RATE = 500000
PAGE_SIZE = 128
ROM_SIZE = 65536
TIMEOUT = 5.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('device', help='serial device to open')
    parser.add_argument('file', help='file to write')
    args = parser.parse_args()

    with open(args.file, 'rb') as f:
        data = f.read(ROM_SIZE)
        more = f.read(1)
        if len(data) != ROM_SIZE or len(more) != 0:
            print(f'Expected {ROM_SIZE} bytes in {args.file}', file=sys.stderr)
            sys.exit(1)

    with ExpectSerial(port=args.device) as dev:
        with tqdm.tqdm(unit='b', unit_scale=True, unit_divisor=1024,
                       total=ROM_SIZE) as t:
            t.set_description('Writing')
            for offset in range(0, ROM_SIZE, PAGE_SIZE):
                t.update(PAGE_SIZE)
                page = offset//PAGE_SIZE
                dev.write(f'write {page}\n'.encode('ascii'))
                dev.expect(b'#')
                dev.write(data[offset:offset+PAGE_SIZE])
                result = dev.expect(b'>')
                if b'OK' not in result:
                    t.close()
                    print(result.replace(b'ready>', b'').decode('ascii'),
                          file=sys.stderr)
                    sys.exit(1)

        with tqdm.tqdm(unit='B', unit_scale=True, unit_divisor=1024,
                       total=ROM_SIZE) as t:
            t.set_description('Verifying')
            for offset in range(0, ROM_SIZE, PAGE_SIZE):
                t.update(PAGE_SIZE)
                page = offset//PAGE_SIZE
                dev.write(f'verify {page}\n'.encode('ascii'))
                dev.expect(b'#')
                dev.write(data[offset:offset+PAGE_SIZE])
                result = dev.expect(b'>')
                if b'OK' not in result:
                    t.close()
                    print(f'page {page} failed verification:', file=sys.stderr)
                    print(result.replace(b'ready>', b'').decode('ascii'),
                          file=sys.stderr)
                    sys.exit(1)


class ExpectSerial(serial.Serial):
    def __init__(self, *args, **kwargs):
        kwargs.setdefault('baudrate', BAUD_RATE)
        kwargs.setdefault('timeout', TIMEOUT)
        super().__init__(*args, **kwargs)

    def expect(self, *characters):
        start = time.monotonic()
        buf = b''
        while time.monotonic() - start < TIMEOUT:
            buf += self.read()
            for char in characters:
                if buf.endswith(char):
                    return buf
        raise TimeoutError


if __name__ == '__main__':
    main()
