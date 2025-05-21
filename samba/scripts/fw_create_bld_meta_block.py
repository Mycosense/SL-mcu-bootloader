#!/usr/bin/env python3
import sys
import struct
import binascii


METABLOCK_SIZE = 0x100
MAGIC = b'\x01\x05'


def crc16_ccitt(data: bytes, initial=0) -> int:
    return binascii.crc_hqx(data, initial)


def generate_bld_metablock(binary: bytes) -> bytes:
    crc = crc16_ccitt(binary)
    metablock_unpadded = MAGIC + struct.pack('<HI', crc, len(binary))
    metablock = metablock_unpadded + bytes(METABLOCK_SIZE - len(metablock_unpadded))
    return metablock


def pack_firmware_for_bld(bin_file_in: str, bin_file_out=''):
    if not bin_file_out:
        bin_file_out = bin_file_in
    print(f'Packing firmware binary. input file: {bin_file_in};  output file: {bin_file_out}')
    with open(bin_file_in, 'rb') as file:
        binary = file.read()
    with open(bin_file_out, 'wb') as file:
        metablock = generate_bld_metablock(binary)
        file.write(metablock)
        file.write(binary)


if __name__ == '__main__':
    file_in = sys.argv[1]
    if len(sys.argv) > 2:
        file_out = sys.argv[2]
    else:
        file_out = file_in
    pack_firmware_for_bld(file_in, file_out)
