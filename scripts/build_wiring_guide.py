"""Build the current minimal CP-100 wiring guide. Requires reportlab."""
from pathlib import Path
import re, csv, hashlib
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.graphics.shapes import Drawing, Rect, String, Line, Polygon, Circle
from wiring_diagrams import photo_pin_map, pads_wiring, dac_wiring

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'output/pdf/CP-100-Wiring-4-Pad-WiFi.pdf'
OUT.parent.mkdir(parents=True,exist_ok=True)
INK=colors.HexColor('#1b2929');TEAL=colors.HexColor('#367e75');PAPER=colors.HexColor('#f0eee7');LINE=colors.HexColor('#ced6ce');MUTED=colors.HexColor('#566560');PINK=colors.HexColor('#f0d2d9')
W,H=A4;WIDTH=W-80
styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyCP',fontName='Helvetica',fontSize=10,leading=14,textColor=INK,spaceAfter=8))
styles.add(ParagraphStyle(name='SmallCP',parent=styles['BodyCP'],fontSize=8.5,leading=11.5,spaceAfter=5))
styles.add(ParagraphStyle(name='HeadCP',fontName='Helvetica-Bold',fontSize=23,leading=28,textColor=INK,spaceAfter=13))
styles.add(ParagraphStyle(name='SubCP',fontName='Helvetica-Bold',fontSize=12,leading=16,textColor=TEAL,spaceBefore=10,spaceAfter=7))
styles.add(ParagraphStyle(name='CellCP',parent=styles['SmallCP'],spaceAfter=0))
styles.add(ParagraphStyle(name='HeaderCellCP',parent=styles['CellCP'],textColor=colors.white,fontName='Helvetica-Bold'))
styles.add(ParagraphStyle(name='CodeCP',fontName='Courier',fontSize=8.5,leading=13,textColor=INK,backColor=PAPER,borderPadding=9,spaceAfter=12))
P=lambda s,style='BodyCP':Paragraph(s,styles[style])
flow=[]
def p(s):flow.append(P(s))
def title(n,s):flow.extend([P(f'0{n} / PETUNJUK RAKIT','SmallCP'),P(s,'HeadCP')])
def sub(s):flow.append(P(s,'SubCP'))
def table(rows,widths):
    t=Table([[P(str(c),'HeaderCellCP' if i==0 else 'CellCP') for c in row] for i,row in enumerate(rows)],colWidths=widths,repeatRows=1,hAlign='LEFT')
    t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),INK),('TEXTCOLOR',(0,0),(-1,0),colors.white),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,PAPER]),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),9),('RIGHTPADDING',(0,0),(-1,-1),9),('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7),('LINEBELOW',(0,0),(-1,0),1,TEAL)]))
    flow.append(t);flow.append(Spacer(1,9))
def box(d,x,y,w,h,heading,caption):
    d.add(Rect(x,y,w,h,fillColor=PAPER,strokeColor=LINE,rx=5,ry=5))
    d.add(String(x+10,y+h-19,heading,fontName='Helvetica-Bold',fontSize=10,fillColor=INK))
    d.add(String(x+10,y+12,caption,fontName='Helvetica',fontSize=8,fillColor=MUTED))
def arrow(d,x1,y1,x2,y2,label=None):
    d.add(Line(x1,y1,x2,y2,strokeColor=TEAL,strokeWidth=1.5))
    if x2>x1:d.add(Polygon([x2,y2,x2-6,y2+3,x2-6,y2-3],fillColor=TEAL,strokeColor=TEAL))
    elif y2<y1:d.add(Polygon([x2,y2,x2-3,y2+6,x2+3,y2+6],fillColor=TEAL,strokeColor=TEAL))
    if label:d.add(String((x1+x2)/2,(y1+y2)/2+8,label,fontName='Helvetica',fontSize=8,textAnchor='middle',fillColor=TEAL))

def footer(c,doc):
    c.setFillColor(INK);c.setFont('Helvetica-Bold',8);c.drawString(40,H-30,'HOUSE OF EXP / ATMOS')
    c.setStrokeColor(LINE);c.line(40,43,W-40,43)
    c.setFont('Helvetica',7.5);c.setFillColor(MUTED);c.drawString(40,29,'CP-100 | Rev02 | 4 pad + SERVICE HP | 01 NOV 2026 | tanpa display / LED eksternal')
    c.drawRightString(W-40,29,f'{doc.page} / 7')

title(1,'Wiring versi minimal')
p('<b>ESP32-S3 N16R8 + 4 pad + PCM5102 + amplifier/shaker.</b> Semua kontrol servis dan pembacaan frekuensi dipindahkan ke HP. MAX7219, WS2812B eksternal, buffer SN74AHCT125, MCP23017, encoder, selector dan SAVE fisik tidak dibutuhkan pada revisi ini.')
d=Drawing(WIDTH,188)
box(d,0,120,130,52,'EMPAT PAD','Switch momentary ke GND')
box(d,190,120,136,52,'ESP32-S3','GPIO 1 / 2 / 4 / 5')
box(d,386,120,129,52,'HP / BROWSER','192.168.4.1')
arrow(d,130,146,190,146,'4 input');arrow(d,326,146,386,146,'Wi-Fi')
box(d,190,31,136,52,'PCM5102 MODULE','I2S ke line-level analog')
box(d,386,31,129,52,'AMPLIFIER','Satu output ke shaker')
arrow(d,258,120,258,83);arrow(d,326,57,386,57,'L / GND')
d.add(String(0,66,'MUTE fisik: opsional',fontSize=9,fontName='Helvetica-Bold',fillColor=INK))
d.add(String(0,50,'GPIO16 -> switch -> GND',fontSize=8.5,fontName='Helvetica',fillColor=MUTED))
d.add(String(0,8,'Sinyal audio dibuat di ESP32, bukan di HP. Ground logika/audio harus memiliki referensi bersama.',fontName='Helvetica',fontSize=8,fillColor=MUTED))
flow.append(d)
sub('Komponen yang disiapkan')
table([['Komponen','Jumlah / catatan'],['ESP32-S3 DevKit / N16R8','1; flash 16 MB, PSRAM 8 MB. Cocokkan marking modul aktual.'],['PCM5102 breakout','1; versi dengan regulator dan label VIN yang jelas.'],['Tombol momentary NO','4 pad; resistor pull-up 10k ohm per pad. MUTE kelima opsional.'],['Amplifier + shaker/speaker','Ikuti spesifikasi input, catu, impedansi dan daya unit aktual.'],['Catu DC dan kabel','5 V teratur untuk logika; catu amplifier sesuai modul, bukan dari GPIO.']],[180,WIDTH-180])
p('<b>Status:</b> pin mengikuti firmware minimal di repo. Build/tes host bukan uji rakitan fisik. Diagram bukan skala mekanik. PDF Rev08 lama masih memuat wiring delapan pad dan panel servis.')
flow.append(PageBreak())
title(2,'Ini pin yang dicolok')
p('<b>Foto tampak atas ESP32-S3 DevKit N16R8.</b> Antena di atas, dua USB di bawah. Lingkaran menunjuk terminal header yang dipakai. GND dan 3V3 dibagi melalui rail/terminal distribusi; jangan memasukkan beberapa kabel longgar ke satu lubang.')
flow.append(photo_pin_map())
flow.append(P('<b>* 5V:</b> dipakai hanya untuk mode catu eksternal, saat daya USB dilepas. <b>* MUTE:</b> tombol opsional. Pin header lengkap dan rangkaian kabel ada pada halaman berikutnya.','SmallCP'))
flow.append(P('Foto referensi pemasok [3], bukan foto rakitan CP-100. Hak foto tetap pada pemiliknya. Board clone dapat berbeda: cocokkan tulisan pin sebelum memasang kabel. Papan DevKit dapat memiliki LED bawaan; tidak perlu menambahkan display atau LED eksternal.','SmallCP'))
flow.append(PageBreak())
title(3,'Kabel empat pad ke DevKit')
p('Gambar board mengikuti posisi header <b>J1 kiri / J3 kanan</b>, dilihat dari sisi komponen. Setiap pad memakai <b>dua terminal switch NO</b>: satu ke GPIO, satu ke GND. Tambahkan satu resistor 10k dari GPIO ke rail 3V3 per tombol.')
flow.append(pads_wiring())
flow.append(P('<b>Hubungkan rail:</b> J1 paling atas (3V3) ke semua ujung atas resistor. J1 paling bawah (GND) ke sisi GND semua tombol. Semua label 3V3 berarti satu rail; semua label GND berarti satu rail. MUTE GPIO16 boleh tidak dipasang.','SmallCP'))
flow.append(P('Pada switch tactile empat kaki, gunakan pasangan yang terbuka saat dilepas. Periksa continuity; jangan memilih dua kaki yang sudah tersambung internal. Diagram kontak dan tabel lengkap ada di halaman 5.','SmallCP'))
flow.append(PageBreak())
title(4,'Kabel DevKit ke PCM5102')
p('Ikuti garis dari pin header ESP32 ke <b>nama terminal</b> pada PCM5102. Bentuk konektor di diagram disederhanakan; urutan pada modul lain bisa berbeda. Foto modul membantu mengenali label SCK/BCK/DIN/LRCK/GND/VIN. Garis bersilangan tanpa titik sambungan tidak saling terhubung.')
flow.append(dac_wiring())
flow.append(P('<b>SCK ke GND</b>; bukan ke GPIO18. <b>BCK</b> yang ke GPIO18. Atur FMT/DEMP/FLT ke LOW dan XSMT ke HIGH 3V3 sesuai jumper modul. VIN 5 V hanya untuk breakout dengan regulator yang mengizinkannya; IC telanjang memakai suplai sesuai datasheet.','SmallCP'))
flow.append(P('<b>Audio keluar:</b> L OUT DAC ke input LINE L/SIGNAL amplifier; AGND/GND DAC ke signal GND amplifier. Shaker ke output speaker amplifier. Lihat halaman 6 untuk detail daya dan output; output speaker bukan GND logika.','SmallCP'))
flow.append(PageBreak())
title(5,'Tabel pin dan rangkaian pad')
p('Gunakan label <b>GPIO</b> pada board, bukan urutan nomor lubang header. Letak header dapat berbeda pada board clone. Semua input tombol bekerja <b>aktif LOW</b>.')
rows=[['Dari ESP32','Ke komponen','Fungsi'],['GPIO1','Pad 1 -> GND','Tahan = preset bunyi'],['GPIO2','Pad 2 -> GND','Tahan = preset bunyi'],['GPIO4','Pad 3 -> GND','Tahan = preset bunyi'],['GPIO5','Pad 4 -> GND','Tahan = preset bunyi'],['GPIO16 (opsional)','MUTE -> GND','Hanya mute; tidak menjadi TEST'],['GPIO18','PCM5102 BCK / BCLK','Bit clock I2S'],['GPIO21','PCM5102 LCK / LRCK','Clock kanal I2S'],['GPIO47','PCM5102 DIN','Data I2S dari ESP32'],['GND','Pad + DAC GND','Referensi ground bersama']]
table(rows,[108,186,WIDTH-294])
sub('Satu rangkaian ini diulang untuk setiap pad')
d=Drawing(WIDTH,123)
d.add(String(216,111,'3V3',fontName='Helvetica-Bold',fontSize=9,fillColor=INK))
d.add(Line(232,104,232,90,strokeColor=INK))
d.add(Rect(224,61,16,29,fillColor=PAPER,strokeColor=INK))
d.add(String(251,72,'10k ohm pull-up',fontSize=9,fontName='Helvetica',fillColor=INK))
d.add(Line(232,61,232,44,strokeColor=INK));d.add(Circle(232,44,2.4,fillColor=TEAL,strokeColor=TEAL))
d.add(Line(65,44,232,44,strokeColor=TEAL,strokeWidth=1.5));d.add(String(10,40,'GPIO',fontSize=10,fontName='Helvetica-Bold',fillColor=INK))
d.add(Line(232,44,355,44,strokeColor=INK));d.add(Circle(355,44,2,strokeColor=INK,fillColor=INK));d.add(Line(356,46,379,61,strokeColor=INK));d.add(Circle(386,44,2,strokeColor=INK,fillColor=INK));d.add(Line(386,44,438,44,strokeColor=INK));d.add(String(446,40,'GND',fontSize=9,fontName='Helvetica-Bold',fillColor=INK))
d.add(String(343,16,'Switch NO / momentary',fontSize=8,fontName='Helvetica',fillColor=MUTED));flow.append(d)
p('Gunakan dua terminal yang <b>terbuka saat dilepas</b> dan tersambung saat ditekan. Pada tactile empat kaki, dua kaki dapat sudah tersambung internal: tentukan pasangan dengan continuity meter sebelum menyolder. Pull-up hanya ke <b>3V3</b>, bukan 5 V.')
p('GPIO6-9 tidak lagi dipakai pad. GPIO38-41 tidak dikendalikan untuk LED/display. Jangan memakai pin PSRAM 35-37 atau pin UART 43/44 untuk sambungan di atas. Jangan memindahkan input ke pin strapping tanpa memeriksa datasheet. [1]')
flow.append(PageBreak())
title(6,'DAC, daya dan amplifier')
sub('PCM5102: tiga sinyal I2S, tanpa level shifter')
p('BCK, LCK dan DIN menerima logika 3,3 V dari ESP32. Firmware mengirim 48 kHz, slot 32-bit; kanal L dan R membawa sinyal mono yang sama. Untuk satu amplifier/shaker, pakai <b>satu</b> line output, misalnya L. Jangan menyatukan L dan R. [2]')
table([['Pin / setting DAC','Hubungkan / pilih'],['BCK / LCK / DIN','GPIO18 / GPIO21 / GPIO47'],['SCK (MCLK)','GND; PLL internal memakai BCK. Jangan tertukar dengan BCK.'],['FMT / DEMP / FLT','LOW / LOW / LOW. FMT LOW memilih I2S.'],['XSMT','HIGH ke 3V3 untuk unmute. Jangan dibiarkan mengambang.'],['VIN pada breakout ber-regulator','5 V hanya bila spesifikasi modul mengizinkan; bare IC bukan input 5 V.'],['GND / AGND','Ground logika dan ground sinyal amplifier.'],['L line out','Ke input line L / signal amplifier; R boleh tidak dihubungkan.']],[178,WIDTH-178])
p('Nomor jumper H/L pada modul GY-PCM5102 dapat berbeda. Ikuti <b>nama sinyal</b> dan skematik modul. Data sheet IC menetapkan suplai sekitar 3,3 V; jangan menerapkan 5 V langsung ke pin suplai bare IC atau pin kontrolnya. [2]')
sub('Dua cara memberi daya ESP32: pilih salah satu')
p('<b>Saat flash/bench:</b> daya ESP32 dari port USB-to-UART. Jangan sekaligus memasukkan 5 V eksternal ke pin 5V board. DAC boleh memakai catu terpisah yang sesuai modul, dengan GND disambungkan. [1]')
p('<b>Saat operasi:</b> bila memakai 5 V eksternal ke pin 5V/GND DevKit, lepaskan daya USB atau gunakan solusi isolasi daya yang sesuai skematik board. Distribusikan catu logika dan amplifier melalui cabang masing-masing; jangan mengalirkan arus shaker lewat board ESP32. [1]')
sub('Sambungan amplifier/shaker')
p('DAC L -> input LINE/SIGNAL amplifier. DAC GND -> input signal ground amplifier. Shaker -> terminal output speaker pasangan kanal yang dipakai. Supply amplifier mengikuti marking dan polaritas unit aktual (YS-502MT harus dikonfirmasi); jangan menebak tegangan catu dari nama produknya.')
p('<b>Jangan</b> memakai output speaker amplifier sebagai GND ESP32, menyatukan output L/R amplifier, atau menyambungkan shaker langsung ke DAC/GPIO. Lepaskan daya saat mengubah wiring. Mulai uji dari MUTE dan level rendah.')
flow.append(PageBreak())
title(7,'Flash, lalu uji bertahap')
p('Gunakan repository privat <b>github.com/dissakamajaya/cp-100</b> dengan akun GitHub yang memiliki akses. Firmware ini untuk <b>ESP32-S3 N16R8</b>; jangan memilih board ESP32 generik.')
sub('Pilihan A - build dan flash dari source')
flow.append(Spacer(1,8))
flow.append(P('gh repo clone dissakamajaya/cp-100<br/>cd cp-100<br/>pio run -d firmware -t upload<br/>pio device monitor -d firmware','CodeCP'))
p('Sambungkan kabel data ke port berlabel <b>UART</b>. Bila autodetect gagal, tambahkan <font name="Courier">--upload-port COM5</font> atau port serial aktual. Bila perlu masuk download mode: tahan BOOT, tekan/lepas RESET, lalu lepas BOOT. [1]')
sub('Pilihan B - paket binary dari repo')
p('Unduh folder/ZIP <font name="Courier">firmware/releases/remote-v1.0.0</font>. Baca README di paket dan jalankan <font name="Courier">python flash.py --check</font> sebelum flash. File terpisah beserta offset disertakan; jangan menulis application-only <font name="Courier">firmware.bin</font> ke alamat 0x0.')
sub('Urutan uji')
for s in [
'1. ESP32 saja: upload, buka serial 115200, ketik <b>wifi</b>. Catat password per perangkat untuk operator; jangan masukkan password ke repo.',
'2. HP: sambungkan ke <b>ATMOS-CP-300-EXP</b>, tetap tersambung meski tanpa internet. Buka <b>http://192.168.4.1</b> lalu Hubungkan kontrol. Boot dan masuk SERVICE harus mute.',
'3. Pasang DAC dan cek sinyal lewat input line speaker aktif/alat ukur, level rendah. TEST dan MUTE harus bekerja. Berikutnya pasang empat pad; tahan bunyi, lepas mute.',
'4. Simpan preset, matikan/nyalakan, lalu cek preset/sweep tetap tersimpan dan boot tetap mute. Tutup tab/putus Wi-Fi saat TEST: output servis harus mute paling lambat setelah lease 2,5 detik.',
'5. Baru pasang amplifier dan shaker. Tentukan sweep/level serta langkah GATHER pada pelat nyata. Batas tersimpan lama tidak diperluas otomatis ke 4000 Hz.'
]:flow.append(P(s,'SmallCP'))
sub('Sumber dan batas dokumen')
flow.append(P('[1] <link href="https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html" color="#367e75">Espressif: ESP32-S3-DevKitC-1 User Guide v1.1</link> - GPIO, USB, daya.','SmallCP'))
flow.append(P('[2] <link href="https://www.ti.com/lit/ds/symlink/pcm5102a.pdf" color="#367e75">Texas Instruments: PCM510xA data sheet, SLAS859C</link> - I2S, PLL, FMT, XSMT, level/suplai IC.','SmallCP'))
flow.append(P('[3] <link href="https://elektroweb.pl/pl/esp32/1312-modul-esp32-s3-devkitc-1-wroom-1-n16r8-16mb-flash-wifi-bluetooth-usb-c.html" color="#367e75">Elektroweb: foto referensi DevKit N16R8</link>. [4] <link href="https://www.ebay.com/itm/314748193002" color="#367e75">Foto referensi GY-PCM5102</link>. Foto telah ada dalam bahan proyek; hak tetap pada pemilik, tidak berlisensi bebas.','SmallCP'))
flow.append(P('Sumber pin proyek: firmware/include/pins.h; konfigurasi I2S: firmware/src/audio.cpp. Header J1/J3 dicocokkan dengan [1]. Pinout dan diagram tidak menggantikan pemeriksaan modul aktual; dokumen ini belum menjadi bukti uji perangkat fisik.','SmallCP'))

doc=SimpleDocTemplate(str(OUT),pagesize=A4,rightMargin=40,leftMargin=40,topMargin=58,bottomMargin=58,title='CP-100 Wiring Rev02 - Foto DevKit dan Diagram Kabel',author='House of EXP',pageCompression=1)
doc.build(flow,onFirstPage=footer,onLaterPages=footer)
# Keep an editable machine-readable wire list beside the PDF.
pins=(ROOT/'firmware/include/pins.h').read_text()
assert re.search(r'PIN_PADS\[4\].*\{1, 2, 4, 5\}',pins)
for name,pin in [('PIN_I2S_BCK',18),('PIN_I2S_LRCK',21),('PIN_I2S_DOUT',47),('PIN_MUTE',16)]:
    assert re.search(rf'{name}\s*=\s*{pin}\s*;',pins)
with OUT.with_suffix('.csv').open('w',newline='') as f:
    writer=csv.writer(f,lineterminator="\n");writer.writerows(rows)
print(OUT)
print('SHA256',hashlib.sha256(OUT.read_bytes()).hexdigest())
