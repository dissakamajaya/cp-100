"""Package built ESP32-S3 images and offline flashing tools without NVS padding."""
import argparse,hashlib,json,os,shutil,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--framework-dir',type=Path,default=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/framework-arduinoespressif32')
args=parser.parse_args()
release=ROOT/'firmware/releases/remote-v1.1.0';release.mkdir(parents=True,exist_ok=True)
build=ROOT/'firmware/.pio/build/cp100'
parts=[]
for offset,name,source in [(0,'bootloader.bin',build/'bootloader.bin'),(0x8000,'partitions.bin',build/'partitions.bin'),(0xe000,'boot_app0.bin',args.framework_dir/'tools/partitions/boot_app0.bin'),(0x10000,'firmware.bin',build/'firmware.bin')]:
    shutil.copyfile(source,release/name);raw=(release/name).read_bytes()
    parts.append({'file':name,'offset':offset,'size':len(raw),'sha256':hashlib.sha256(raw).hexdigest()})
# Bound each image to its intended region. No image occupies the NVS partition.
for part,end in zip(parts,[0x8000,0x9000,0x10000,0x650000]):assert part['offset']+part['size']<=end
files=sorted([p for d in ['src','include','web','tools'] for p in (ROOT/'firmware'/d).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='web_assets.h'])+[ROOT/'firmware/platformio.ini']
source={str(p.relative_to(ROOT/'firmware')):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
source_hash=hashlib.sha256(json.dumps(source,sort_keys=True).encode()).hexdigest()
pdf=ROOT/'output/pdf/CP-100-Wiring-4-Pad-WiFi.pdf';assert pdf.is_file()
manifest={'documentation':{'revision':'Rev04','pdf':pdf.name,'sha256':hashlib.sha256(pdf.read_bytes()).hexdigest()},'release':'remote-v1.1.0','date':'2026-11-01','chip':'ESP32-S3','board':'N16R8','flash_size':'16MB','psram':'8MB OPI','hardware_tested':False,'pad_max_hold_ms':5000,'factory_preset_hz':[1096,1360,2249,2386],'wifi_password_policy':'owner-selected shared operator passphrase','display':False,'external_leds':False,'ssid':'ATMOS-CP-300-EXP','address':'192.168.4.1','nvs_region_written':False,'parts':parts,'source_sha256':source,'source_tree_sha256':source_hash}
(release/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(release/'SHA256SUMS').write_text(''.join(f'{p["sha256"]}  {p["file"]}\n' for p in parts))
archive=ROOT/'firmware/releases/CP-100-remote-v1.1.0.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(release.iterdir()):
        if p.is_file():z.write(p,p.name)
    z.write(pdf,pdf.name)
print(archive)
print('SHA256',hashlib.sha256(archive.read_bytes()).hexdigest())
