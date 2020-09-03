#!/usr/bin/env python
import argparse
import sys
import time

import serial
import tqdm

BAUD_RATE = 500000
PAGE_SIZE = 128
ROM_SIZE = 65536
TIMEOUT = 5.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verify-only', action='store_true',
                        help='verify the file without writing')
    parser.add_argument('device', help='serial device to open')
    parser.add_argument('file', help='file to write/verify')
    args = parser.parse_args()

    with open(args.file, 'rb') as f:
        data = f.read(ROM_SIZE)
        more = f.read(1)
        if len(data) != ROM_SIZE or len(more) != 0:
            print(f'Expected {ROM_SIZE} bytes in {args.file}', file=sys.stderr)
            sys.exit(1)

    with ExpectSerial(port=args.device) as dev:
        try:
            dev.expect(b'>')
        except TimeoutError as e:
            print(f'{e}\ncontinuing anyway...', file=sys.stderr)
        if not args.verify_only:
            send_all_pages(dev, data, command='write', description='Writing')
        send_all_pages(dev, data, command='verify', description='Verifying')


def send_all_pages(device: 'ExpectSerial',
                   data: bytes,
                   command: str,
                   description: str):
    with tqdm.tqdm(unit='b', unit_scale=True, unit_divisor=1024,
                   total=ROM_SIZE) as t:
        t.set_description(description)
        for offset in range(0, ROM_SIZE, PAGE_SIZE):
            page = offset // PAGE_SIZE
            device.write(f'{command} {page}\n'.encode('ascii'))
            device.expect(b'#')
            device.write(data[offset:offset+PAGE_SIZE])
            result = device.expect(b'>')
            t.update(PAGE_SIZE)
            if b'OK' not in result:
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


if __name__ == '__main__':
    main()
