#!/usr/bin/env python3
"""Verify this release, then flash only the four named regions (not NVS)."""
import argparse, hashlib, json
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',help='Serial UART port, e.g. COM5 or /dev/cu.usbserial-...')
    parser.add_argument('--baud',type=int,default=460800)
    parser.add_argument('--check',action='store_true',help='Verify SHA256 and layout without connecting to a board')
    args=parser.parse_args()
    root=Path(__file__).resolve().parent
    manifest=json.loads((root/'manifest.json').read_text())
    expected={0:'bootloader.bin',0x8000:'partitions.bin',0xe000:'boot_app0.bin',0x10000:'firmware.bin'}
    parts=manifest['parts']
    if {p['offset']:p['file'] for p in parts}!=expected or len(parts)!=4:
        parser.error('Unexpected flash layout')
    for p in parts:
        path=root/p['file'];raw=path.read_bytes()
        if len(raw)!=p['size'] or hashlib.sha256(raw).hexdigest()!=p['sha256']:
            parser.error(f'Corrupt or mixed release file: {p["file"]}')
    print('PASS: ESP32-S3 N16R8 release checksums and flash offsets verified.')
    if args.check:return
    if not args.port:parser.error('--port is required to flash; use --check to verify files only')
    try:import esptool
    except ImportError:parser.error('Install requirements first: python -m pip install -r requirements.txt')
    command=['--chip','esp32s3','--port',args.port,'--baud',str(args.baud),'--before','default_reset','--after','hard_reset','write_flash','--flash_size','16MB']
    for p in sorted(parts,key=lambda p:p['offset']):command += [hex(p['offset']),str(root/p['file'])]
    print('Writing bootloader/partitions/OTA initializer/app; no full-chip erase and no NVS image.')
    esptool.main(command)

if __name__=='__main__':main()
