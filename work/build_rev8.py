from pathlib import Path
import sys, json, csv, io, math, zipfile, hashlib
sys.path.insert(0,str(Path(__file__).resolve().parent))
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'outputs'; BUILD=OUT/'CP-100-Rev08-Blueprint';BUILD.mkdir(exist_ok=True)
s=(ROOT/'work/build_rev5_concise.py').read_text().split('# 1\n')[0]
s=s.replace("PATH=OUT/'Chladni-08-Rev05-Desain-dan-Wiring.pdf'", "PATH=ROOT/'work/rev8/new-pages.pdf'").replace('CHLADNI / 08','CP-100').replace('Rev.05','Rev.08').replace('/ 06','/ 13')
exec(compile(s,__file__,'exec'))
from reportlab.lib.units import mm
from pypdf import PdfReader,PdfWriter

def note(t,y=530):para(t,36,y,770,9,28,MUTED)
def image_fit(path,x,y,w,h):
 path=Path(path)
 if not path.is_absolute():path=ROOT/path
 im=ImageReader(str(path));iw,ih=im.getSize();sc=min(w/iw,h/ih);c.drawImage(im,x+(w-iw*sc)/2,H-y-(h+ih*sc)/2,iw*sc,ih*sc,mask='auto')
def dim(x,y,xx,yy,label):
 line(x,y,xx,yy,MUTED,.6)
 for px,py in[(x,y),(xx,yy)]:
  if y==yy:line(px,py-4,px,py+4,MUTED,.6)
  else:line(px-4,py,px+4,py,MUTED,.6)
 txt(label,(x+xx)/2+ (10 if x==xx else 0),(y+yy)/2-12,8,'Mono',MUTED,'left' if x==xx else 'center')
def cross(x,y,r=4):line(x-r,y,x+r,y,'#9BA295',.4);line(x,y-r,x,y+r,'#9BA295',.4)
visitor={'name':'visitor','w':220,'h':122,'holes':[(7,7,3.4),(213,7,3.4),(7,115,3.4),(213,115,3.4)],'rects':[(x-20.5,y-20.5,41,41,2) for y in [37,85] for x in [38,86,134,182]],'circles':[]}
service={'name':'service','w':260,'h':180,'holes':[(8,8,3.4),(252,8,3.4),(8,172,3.4),(252,172,3.4)],'rects':[(65,16,130,34,1),(70,122,10,6,1),(180,122,10,6,1),(42,154.5,6,5,1)],'circles':[(52,85,7.2),(130,85,7.2),(208,85,7.2),(104,157,12.6),(156,157,12.6)]}

def panel(p,x,y,sc,detail=True):
 def rr(a,b,w,h,fill,stroke=None,r=0):rect(x+a*sc,y+b*sc,w*sc,h*sc,fill,stroke,r*sc)
 rr(0,0,p['w'],p['h'],'#101010','#333333',3)
 for a,b,w,h,r in p['rects']:rr(a,b,w,h,'#F6F5F0','#777777',r)
 for a,b,d in p['holes']+p['circles']:circle(x+a*sc,y+b*sc,d/2*sc,'#F6F5F0','#888888');cross(x+a*sc,y+b*sc)
 if not detail:return
 if p['name']=='visitor':
  for i,(a,b,_,_,_) in enumerate(p['rects']):
   rr(a+.5,b+.5,40,40,'#F0F0EA','#FFFFFF',1.5);txt(str(i+1),x+(a+4)*sc,y+(b+4)*sc,9,'Mono','#333333')
  txt('TEKAN & TAHAN',x+110*sc,y+116*sc,6,'Mono','#BBBBBB','center')
 else:
  matrix(x+66*sc,y+17*sc,128*sc,32*sc)
  for a,l in[(52,'FREQ'),(130,'LEVEL'),(208,'FOLD')]:
   circle(x+a*sc,y+85*sc,11*sc,'#202020','#777777');line(x+a*sc,y+76*sc,x+a*sc,y+81*sc,'#FFFFFF',1.4);txt(l,x+a*sc,y+101*sc,8,'Mono','#FFFFFF','center')
  for a,l,vals in[(75,'WAVE','SINE   SAW   TRI'),(185,'HARMONICS','ODD  EVEN  O+E')]:
   txt(l,x+a*sc,y+112*sc,7,'Mono','#FFFFFF','center');txt(vals,x+a*sc,y+134*sc,6,'Mono','#BBBBBB','center')
  for a,l in[(104,'SAVE'),(156,'TEST / MUTE')]:circle(x+a*sc,y+157*sc,6*sc,'#222222','#999999');txt(l,x+a*sc,y+145*sc,6.5,'Mono','#FFFFFF','center')
  txt('SERVICE / RUN',x+45*sc,y+146*sc,6,'Mono','#FFFFFF','center')

# 1
page(1,'CP-100 / instalasi')
image_fit(ROOT/'work/rev8/reference-render.png',36,115,455,320)
for yy,a,b in [(126,'PELAT','600 x 600 mm'),(211,'KACA','Diameter 500 mm'),(296,'CASING ACUAN','800 x 820 x 350 mm')]:txt(a,520,yy,9,'Mono',MUTED);txt(b,520,yy+21,21,'Bold')
line(36,451,806,451)
para('<b>Pengunjung:</b> 8 pad momentary. Tahan = bunyi; lepas = mute.<br/><b>Servis:</b> FREQ, LEVEL, FOLD, WAVE, HARMONICS, SAVE, TEST/MUTE. Di dalam casing, lewat pintu belakang berkunci.',36,470,455,11,73)
para('<b>Beban:</b> sepatu > kaca > rangka casing.<br/>Kaca tidak bertumpu pada pelat.<br/><b>Celah awal:</b> 30 mm dari pelat ke bawah kaca; kurangi ruang pasir dan gerak pelat saat uji.',520,462,286,10,85)
end()
# 2
page(2,'Referensi casing dari klien')
image_fit(ROOT/'work/rev8/reference-dimensions.png',36,113,430,396)
image_fit(ROOT/'work/rev8/reference-render.png',478,130,328,340)
note('Gambar asli. Controller komersial pada gambar diganti panel 8 pad. Panel servis dipindah ke dalam casing. Pelat final 600 x 600 mm.')
end()
# 3
page(3,'Panel pengunjung')
txt('TAMPAK DEPAN / mm / SKALA 1:1,49',36,111,8,'Mono',MUTED)
x,y,sc=65,160,1.9;panel(visitor,x,y,sc);dim(x,y-19,x+220*sc,y-19,'220');dim(x-16,y,x-16,y+122*sc,'122')
dim(x+38*sc,y+133*sc,x+86*sc,y+133*sc,'48 PITCH');txt('8 x 41 x 41 / R2',x+140*sc,y+136*sc,8,'Mono',MUTED,'center')
table(518,124,288,['DETAIL','UKURAN / mm'],[('Panel aluminium hitam','220 x 122 x 2'),('Cap opal PC, 8 buah','40 x 40 x 2'),('Jendela cap','41 x 41 / R2'),('Mount panel','4 x diameter 3,4'),('Pusat lubang X','7 ; 213'),('Pusat lubang Y','7 ; 115'),('Pusat pad X','38 ; 86 ; 134 ; 182'),('Pusat pad Y','37 ; 85'),('Bukaan casing','200 x 102'),('Rebate muka casing','220,5 x 122,5')],[128,160],rowh=30,size=9)
para('<b>Operasi:</b> standby putih. Saat ditahan: pink atau tosca. Satu pad aktif; tekan pertama diprioritaskan.',36,457,440,10,45)
note('Datum (0,0): sudut kiri atas, tampak depan. Panel bertumpu pada bibir casing 10 mm. Toleransi rancangan: cutout +/-0,2; pusat lubang +/-0,2 mm.')
end()
# 4
page(4,'Panel servis / internal')
txt('TAMPAK DEPAN / mm / SKALA 1:1,49',36,111,8,'Mono',MUTED)
x,y,sc=60,151,1.9;panel(service,x,y,sc);dim(x,y-18,x+260*sc,y-18,'260');dim(x-16,y,x-16,y+180*sc,'180')
table(586,124,220,['FITUR','X ; Y / mm'],[('Display 130 x 34','65 ; 16 (pojok)'),('FREQ dia 7,2','52 ; 85'),('LEVEL dia 7,2','130 ; 85'),('FOLD dia 7,2','208 ; 85'),('WAVE slot 10 x 6','75 ; 125'),('HARM slot 10 x 6','185 ; 125'),('MODE slot 6 x 5','45 ; 157'),('SAVE dia 12,6','104 ; 157'),('MUTE dia 12,6','156 ; 157'),('4 mount dia 3,4','X 8/252; Y 8/172')],[110,110],rowh=30,size=8.1)
para('Aluminium hitam 2 mm. Knob diameter 22 mm, bore D6. WAVE dan HARMONICS: switch geser tiga posisi tetap. MODE: switch geser dua posisi.',586,463,220,9,62)
note('Datum kiri atas tampak depan. Koordinat slot dan tombol = pusat; display = pojok kiri atas. File 1:1 terpisah, jangan ukur dari halaman ini.')
end()
# 5
page(5,'Rakitan pad / potongan A-A')
txt('POTONGAN PRINSIP / mm / TIDAK BERSKALA',36,115,9,'Mono',MUTED)
# section representative, enlarged
rect(60,200,110,14,'#151515');rect(410,200,110,14,'#151515');rect(177,200,226,14,'#DBDDD7','#85917E');txt('CAP PC 40 x 40 x 2',290,175,9,'Mono',align='center')
rect(166,215,248,8,'#ADB4A8');rect(285,223,13,40,'#ADB4A8');rect(270,266,43,27,'#353535');rect(90,294,400,10,'#788D66');rect(366,278,28,14,'#DCDCD6');
for xx in[150,423]:rect(xx,223,15,69,'#BFC7B8');rect(xx-8,252,31,8,'#788D66')
line(45,353,535,353,INK);line(45,353,45,200,INK);line(535,353,535,200,INK)
for yy,label in[(200,'Z 0 / MUKA'),(214,'Z 2 / PANEL'),(294,'Z 12 / PCB'),(353,'Z 24 / BATAS BOX')]:line(530,yy,558,yy,MUTED);txt(label,565,yy-3,8,'Mono',MUTED)
arrow(230,273,270,279);txt('B3F-1000',140,269,9,'Mono');arrow(410,269,383,281);txt('RGB',419,256,8,'Mono')
para('<b>Komponen:</b> tactile 6 x 6 x 4,3; RGB 5 x 5, offset 12 mm dari pusat pad. PCB custom 198 x 100 x 1,6 mm; pojok pada X=11, Y=11 terhadap panel.',36,388,475,10,56)
para('<b>Pemandu:</b> frame PA12 hitam; flange cap 44 x 44 x 1 mm. Pegas ringan mengembalikan cap. Plunger dan stopper memakai shim untuk penyetelan.',36,458,475,10,56)
para('<b>Setelan prototipe</b><br/>PCB top: Z=12 mm.<br/>Tactile top: Z=7,7 nominal.<br/>Gap plunger idle: 0,1 mm awal.<br/>Travel tactile: 0,15-0,45 mm.<br/><br/>Setel stopper setelah titik kontak terukur. Setelah kontak, beban tekan diterima frame. Cap tidak menahan beban lewat tactile saja.',565,386,241,10,135)
note('Rancangan mekanik acuan. Frame, return spring, shim, dan PCB masih komponen custom; belum berupa paket Gerber atau desain produksi tervalidasi.')
end()
# 6
page(6,'Penempatan panel dan pintu servis')
txt('ATAS / mm / DEPAN DI BAWAH',36,115,8,'Mono',MUTED)
x,y,k=60,151,.405
rect(x,y,800*k,820*k,'#E4E6DD',INK)
rect(x+100*k,y+110*k,600*k,600*k,'#B7BFAF','#7D8975')
circle(x+400*k,y+410*k,250*k,'#D8E3DF','#607267')
rect(x+290*k,y+19*k,220*k,122*k,'#171717')
rect(x+250*k,y+815*k,300*k,5*k,INK)
txt('KACA dia 500',x+400*k,y+380*k,9,'Mono',align='center');txt('PELAT 600 x 600',x+400*k,y+670*k,8,'Mono',align='center');txt('8 PAD',x+400*k,y+58*k,8,'Mono','#FFFFFF','center')
# correct top view orientation: front is top in diagram
rect(36,108,390,22,BG);txt('ATAS / mm / DEPAN DI ATAS',36,115,8,'Mono',MUTED)
dim(x,y-15,x+800*k,y-15,'800');dim(x-15,y,x-15,y+820*k,'820')
txt('PINTU BELAKANG',x+400*k,y+840*k,8,'Mono',MUTED,'center')
para('Pad: pojok X=290, Y=19 dari sudut depan kiri casing. Pelat: X=100, Y=110. Kaca: pusat X=400, Y=410.',36,514,390,9,33)
txt('BELAKANG / PINTU + PANEL INTERNAL',470,115,8,'Mono',MUTED)
rect(470,151,336,147,'#E4E6DD',INK);rect(575,178.3,126,92.4,'#F3F4ED',INK);rect(583.4,186.7,109.2,75.6,'#171717');circle(693,224,3,INK)
dim(575,285,701,285,'300');txt('220',708,217,8,'Mono',MUTED)
para('<b>Pintu:</b> bukaan 300 x 220; X=250, Z=65 dari atas casing. Engsel kiri, kunci kanan. Daun pintu overlap 10 mm.<br/><b>Panel:</b> 260 x 180; X=270, Z=85. Muka panel masuk 25 mm dari sisi dalam pintu. Knob menonjol maksimal 18 mm.',470,321,336,10,99)
para('<b>Box servis:</b> kedalaman rancangan 100 mm, sekat dari ruang pasir; kabel lewat grommet. Matrix 128 x 32 x 14 dijepit carrier, sisakan envelope kabel 168 x 44. Gunakan standoff untuk menyetel tinggi switch.',470,432,336,10,72)
note('Box pad target <=24 mm; overlap proyeksi pelat 31 mm. Periksa datum kaca, ruang pasir, dan amplitudo pelat sebelum potong casing.',530)
end()
# Service rear stack, switch orientation and fastening.
page(7,'Rakitan panel servis')
txt('ENVELOPE KOMPONEN / PROYEKSI DARI DEPAN / mm',36,115,8,'Mono',MUTED)
x,y,k=48,152,1.7
rect(x,y,260*k,180*k,'#E4E6DD',INK,4)
rect(x+46*k,y+8*k,168*k,50*k,'#C8D1C0',INK,2)
rect(x+66*k,y+17*k,128*k,32*k,'#2A3725',INK,1)
for a in[52,130,208]:rect(x+(a-12)*k,y+73*k,24*k,24*k,'#BDC8B4',INK,1)
for a in[75,185]:rect(x+(a-12)*k,y+121*k,24*k,8*k,'#BDC8B4',INK,1)
rect(x+33*k,y+153*k,24*k,8*k,'#BDC8B4',INK,1)
for a in[104,156]:rect(x+(a-10)*k,y+147*k,20*k,20*k,'#BDC8B4',INK,1)
for a,b,d in service['holes']:circle(x+a*k,y+b*k,d/2*k,BG,INK)
txt('MATRIX + RUANG HEADER',x+130*k,y+35*k,8,'Mono','#FFFFFF','center')
para('Envelope rancangan: matrix + konektor 168 x 50; carrier encoder 24 x 24; carrier tombol 20 x 20. Ukuran carrier bukan footprint pabrik.',36,487,455,9,45)
para('<b>DISPLAY</b><br/>Lensa smoked PC 1 mm dari belakang panel. Matrix dijepit carrier; tidak mengebor lubang PCB yang pitch-nya belum diketahui. Carrier ditopang frame ke empat baut panel M3.<br/><br/><b>ENCODER</b><br/>Washer + mur pada bushing M7. PCB terminal di belakang body. Knob D6 diameter 22; push FREQ aktif, push LEVEL/FOLD NC.',535,122,271,10,175)
para('<b>SWITCH GESER</b><br/>C&K OS102/103 memakai aktuator samping. PCB kecil dipasang 90 derajat terhadap muka panel, lewat bracket. Geser aktuator mendatar di slot; shim mengatur tinggi agar tidak bergesek.<br/><br/><b>SAVE / TEST-MUTE</b><br/>Cap diameter 12 dengan flange 15, plunger, return spring dan stopper; tactile B3F di PCB carrier. Setel stroke seperti pad.',535,321,271,10,188)
note('Frame belakang memakai empat mount M3 panel; standoff dan bracket custom. Uji reach aktuator, konektor, dan akses mur pada satu rakitan sebelum produksi.')
end()

# 8
page(8,'Komponen acuan / fabrikasi')
table(36,116,770,['QTY','KOMPONEN / SKU','DIMENSI ACUAN / mm','PEMASANGAN'],[
('1','MAX7219 quad / ShillehTek','128 x 32 x 14','Clamp carrier; pitch PCB tidak diasumsikan'),
('3','Bourns PEC11R-4215F-S0024','Body 12,5 x 13,4; shaft D6 / L15','Bushing M7 x 0,75; lubang panel 7,2'),
('2','C&K OS103011MA7QP1','12,6 x 4,3 x 4; travel 2 + 2','WAVE + HARM; PCB carrier custom'),
('1','C&K OS102011MA1QN1','8,6 x 4,4 x 4; travel 2','RUN / SERVICE; PCB carrier custom'),
('10','Omron B3F-1000','6 x 6; tinggi 4,3 +/-0,2','8 pad + SAVE + TEST/MUTE'),
('8','WS2812B / 5050','5 x 5','PCB pad custom; satu RGB per pad'),
('1+1','Panel visitor + service','220 x 122 x 2 / 260 x 180 x 2','Aluminium hitam; deburr semua cutout'),
('8','Cap + flange pad custom','40 x 40 x 2 / flange 44 x 44 x 1','Opal PC; frame PA12 + return spring'),
('3','Knob custom D6','Diameter 22; tinggi <=18','Bore D6 mengikuti shaft encoder'),
('1','Lensa display smoked PC','140 x 44 x 1','Jendela 130 x 34; dudukan dari belakang')
],[36,240,217,277],rowh=30,size=8.5)
txt('SUMBER DIMENSI / KLIK UNTUK DATASHEET',36,457,8,'Mono',MUTED)
refs=[('Bourns PEC11R','https://www.bourns.com/docs/product-datasheets/pec11r.pdf'),('C&K OS series','https://www.ckswitches.com/media/1428/os.pdf'),('Omron B3F','https://omronfs.omron.com/en_US/ecb/products/pdf/en-b3f.pdf'),('MAX7219 module','https://shillehtek.com/blogs/shillehtek-product-manuals/max7219-4-in-1-dot-matrix-display-module-red-manual'),('WS2812B','https://cdn.sparkfun.com/datasheets/BreakoutBoards/WS2812B.pdf')]
for i,(label,url) in enumerate(refs):
 xx=36+i*155;txt(label,xx,478,9,'Bold');c.linkURL(url,(xx,H-493,xx+145,H-475),thickness=0)
note('Acuan prototipe; komponen belum dibeli. Ukur unit terpilih, uji satu pad dan carrier switch, lalu cocokkan template sebelum fabrikasi seluruh panel.')
end()
# 11 after three retained wiring pages
page(12,'Selector / terminal solder')
table(36,117,770,['KOMPONEN','PIN FISIK','SOLDER KE','FUNGSI'],[
('WAVE / OS103011','2 (COM)','GND','Common'),('WAVE / OS103011','1','MCP pin22 / GPA1','Posisi 1: SINE'),('WAVE / OS103011','3','NC','Posisi 2: SAW'),('WAVE / OS103011','4','MCP pin23 / GPA2','Posisi 3: TRIANGLE'),('HARM / OS103011','2 (COM)','GND','Common'),('HARM / OS103011','1','MCP pin24 / GPA3','Posisi 1: ODD'),('HARM / OS103011','3','NC','Posisi 2: EVEN'),('HARM / OS103011','4','MCP pin27 / GPA6','Posisi 3: ODD + EVEN'),('MODE / OS102011','2 (COM) / 1 / 3','GND / MCP pin21 GPA0 / NC','Posisi 1 SERVICE; posisi 2 RUN'),('FOLD / PEC11R','A / B / C','MCP pin25 GPA4 / pin26 GPA5 / GND','Push NC')],[169,100,245,256],rowh=29,size=9)
para('Semua input MCP: pull-up 10k ke 3V3. WAVE/HARM tengah = kedua input HIGH. Dua input LOW: tahan pilihan sah terakhir. Nomor terminal mengikuti datasheet, bukan urutan warna kabel.',36,445,770,10,42)
para('<b>Simpan:</b> SERVICE > pilih parameter > SAVE > pad tujuan > RUN. Preset: frekuensi, level, wave, fold, harmonik.<br/><b>Sweep:</b> tahan FREQ 2 dtk > MIN > push > MAX > SAVE. Batas final ditentukan dari uji pola pada pelat 600 x 600.',36,505,770,9.5,32)
end();c.save()
# Rebuild photographic cards from the revised component manifest.
c=canvas.Canvas(str(ROOT/'work/rev8/photos.pdf'),pagesize=(W,H))
photos=json.loads((ROOT/'work/rev8/photo-sources.json').read_text())
from urllib.parse import urlparse
for batch in range(2):
 page(13+batch,'Komponen / '+('elektronik' if batch==0 else 'kontrol dan koneksi'))
 for j,item in enumerate(photos[batch*8:(batch+1)*8]):
  xx=36+j%4*196;yy=119+j//4*211
  rect(xx,yy,182,195,'#FFFFFF',LINE,4);image_fit(item['path'],xx+10,yy+9,162,140)
  para(item['label'],xx+10,yy+153,162,9,25)
  txt(urlparse(item['source']).netloc.removeprefix('www.'),xx+10,yy+180,6.8,'Mono',MUTED)
  c.linkURL(item['source'],(xx,H-yy-195,xx+182,H-yy),thickness=0)
 end()
c.save();photo_reader=PdfReader(ROOT/'work/rev8/photos.pdf')
# Assemble retained electronic drawings and photos, re-number footer consistently.
new=PdfReader(ROOT/'work/rev8/new-pages.pdf');old=PdfReader(OUT/'CP-100-Rev07-Desain-dan-Wiring.pdf')
pages=list(new.pages[:8])+list(old.pages[2:5])+[new.pages[8]]+list(photo_reader.pages)
writer=PdfWriter()
for i,p in enumerate(pages):
 b=io.BytesIO();cc=canvas.Canvas(b,pagesize=(W,H));cc.setFillColor(HexColor(BG));cc.rect(35,15,772,19,fill=1,stroke=0);cc.setFont('Mono',8);cc.setFillColor(HexColor(MUTED));cc.drawString(36,20,'CP-100 / REV.08 / ACUAN PROTOTIPE');cc.drawRightString(806,20,f'{i+1:02d} / {len(pages):02d}');cc.save();p.merge_page(PdfReader(b).pages[0]);writer.add_page(p)
writer.add_metadata({'/Title':'CP-100 Rev08 - Blueprint dan Wiring','/Author':'House of EXP'})
path=OUT/'CP-100-Rev08-Blueprint-dan-Wiring.pdf'
with path.open('wb') as f:writer.write(f)
# Export actual-mm geometry, no guessed component PCB hole patterns.
features=[]
for p in[visitor,service]:
 for xx,yy,d in p['holes']+p['circles']:assert d/2<xx<p['w']-d/2 and d/2<yy<p['h']-d/2
 for xx,yy,ww,hh,r in p['rects']:assert xx>=0 and yy>=0 and xx+ww<=p['w'] and yy+hh<=p['h']
 svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{p["w"]}mm" height="{p["h"]}mm" viewBox="0 0 {p["w"]} {p["h"]}"><g fill="none" stroke="#000" stroke-width="0.1">',f'<rect x="0" y="0" width="{p["w"]}" height="{p["h"]}" rx="3"/>']
 for xx,yy,ww,hh,r in p['rects']:svg.append(f'<rect x="{xx}" y="{yy}" width="{ww}" height="{hh}" rx="{r}"/>');features.append([p['name'],'window',xx,yy,ww,hh,r,'top-left'])
 for xx,yy,d in p['holes']+p['circles']:svg.append(f'<circle cx="{xx}" cy="{yy}" r="{d/2}"/>');features.append([p['name'],'hole',xx,yy,d,d,0,'center'])
 svg.append('</g></svg>');(BUILD/f'{p["name"]}-cutout-1to1.svg').write_text('\n'.join(svg))
 # DXF mm including rounded corners as actual arcs.
 dxf=['0','SECTION','2','HEADER','9','$INSUNITS','70','4','0','ENDSEC','0','SECTION','2','ENTITIES']
 def entity(vals):dxf.extend(map(str,vals))
 def dl(x1,y1,x2,y2):entity([0,'LINE',8,'CUT',10,x1,20,p['h']-y1,11,x2,21,p['h']-y2])
 def da(cx,cy,r,start,end):entity([0,'ARC',8,'CUT',10,cx,20,p['h']-cy,40,r,50,start,51,end])
 def dr(xx,yy,ww,hh,r):
  dl(xx+r,yy,xx+ww-r,yy);dl(xx+ww,yy+r,xx+ww,yy+hh-r);dl(xx+ww-r,yy+hh,xx+r,yy+hh);dl(xx,yy+hh-r,xx,yy+r)
  for cx,cy,a,b in[(xx+r,yy+r,90,180),(xx+ww-r,yy+r,0,90),(xx+ww-r,yy+hh-r,270,360),(xx+r,yy+hh-r,180,270)]:da(cx,cy,r,a,b)
 dr(0,0,p['w'],p['h'],3)
 for xx,yy,ww,hh,r in p['rects']:dr(xx,yy,ww,hh,r)
 for xx,yy,d in p['holes']+p['circles']:entity([0,'CIRCLE',8,'CUT',10,xx,20,p['h']-yy,40,d/2])
 dxf+=['0','ENDSEC','0','EOF'];(BUILD/f'{p["name"]}-cutout-1to1.dxf').write_text('\n'.join(dxf)+'\n')
with (BUILD/'cutout-coordinates-mm.csv').open('w') as f:
 wr=csv.writer(f);wr.writerow(['panel','feature','x','y','width_or_diameter','height','corner_radius','datum']);wr.writerows(features)
# Full size print template on A3, with scale calibration square.
tc=canvas.Canvas(str(BUILD/'CP-100-Panel-Templates-1to1-A3.pdf'),pagesize=(420*mm,297*mm))
for p in[visitor,service]:
 tc.setFont('Helvetica',12);tc.drawString(20*mm,277*mm,f'CP-100 / {p["name"].upper()} / REV08 / mm / 1:1')
 tc.saveState();tc.translate(20*mm,245*mm);tc.scale(mm,-mm);tc.setLineWidth(.15);tc.roundRect(0,0,p['w'],p['h'],3,stroke=1,fill=0)
 for xx,yy,ww,hh,r in p['rects']:tc.roundRect(xx,yy,ww,hh,r,stroke=1,fill=0)
 for xx,yy,d in p['holes']+p['circles']:tc.circle(xx,yy,d/2,stroke=1,fill=0)
 tc.restoreState();tc.rect(320*mm,180*mm,50*mm,50*mm);tc.setFont('Helvetica',10);tc.drawString(320*mm,172*mm,'CHECK 50 x 50 mm');tc.drawString(20*mm,40*mm,'Print 100% / Actual size. No Fit to page. Front view, datum top-left.');tc.drawString(20*mm,32*mm,'Prototype reference. Match actual components before cutting.');tc.showPage()
tc.save()
rows=list(csv.reader((OUT/'CP-100-Rev07-Pinout.csv').open()))
for row in rows:
 row[:]=[v.replace('WAVE terminal 3: TRIANGLE','WAVE physical pin 4: TRIANGLE').replace('HARMONICS terminal 3: ODD+EVEN','HARMONICS physical pin 4: ODD+EVEN') for v in row]
 if row[0] in ['SP3T GESER WAVE','SP3T GESER HARMONICS']:row[1]='physical pin 3'
rows += [['C&K OS103011 WAVE/HARM','physical pin 2 / COM','GND','pin1=input left; pin3 NC; pin4=input right'],['C&K OS102011 MODE','physical pin 2 / 1 / 3','GND / GPA0 / NC','pos1 SERVICE; pos2 RUN']]
with (OUT/'CP-100-Rev08-Pinout.csv').open('w') as f:csv.writer(f).writerows(rows)
(BUILD/'README.txt').write_text('CP-100 REV08 - REFERENCE PROTOTYPE\nUnits mm. SVG and A3 PDF front view: origin top-left; X right, Y down. DXF uses bottom-left, Y up. CUT layer includes panel perimeter and holes; no kerf compensation.\nPanel material: 2mm black aluminium. Cutouts +/-0.2mm; centers +/-0.2mm proposed. Deburr; match units before full fabrication.\nCustom cap, frame, spring, carrier and PCB need prototype fit checks. This package does not include Gerbers or production-ready 3D parts. See main PDF for assembly stack and part basis.\n')
import shutil
shutil.copy(path,BUILD/path.name);shutil.copy(OUT/'CP-100-Rev08-Pinout.csv',BUILD/'CP-100-Rev08-Pinout.csv')
with zipfile.ZipFile(OUT/'CP-100-Rev08-Blueprint-Build.zip','w',zipfile.ZIP_DEFLATED) as z:
 for f in BUILD.iterdir():z.write(f,f.name)
print(json.dumps({'pdf':str(path),'pages':len(pages),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'cut_features':len(features)}))
