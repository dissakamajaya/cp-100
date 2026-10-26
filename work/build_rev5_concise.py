from pathlib import Path
import math,csv,io
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import Paragraph
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.utils import ImageReader
from reportlab.graphics import renderPDF
from svglib.svglib import svg2rlg
from rev3_data import PINS,LEFT,RIGHT,BY_GPIO,COLORS
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'outputs';PATH=OUT/'Chladni-08-Rev05-Desain-dan-Wiring.pdf'
for name,file in [('Sans','Arial.ttf'),('Bold','Arial Bold.ttf'),('Mono','Andale Mono.ttf')]:pdfmetrics.registerFont(TTFont(name,'/System/Library/Fonts/Supplemental/'+file))
W,H=841.89,595.28;BG='#F6F5F0';INK='#28332C';MUTED='#758071';LINE='#D4D9CC';ACCENT='#A87536'
logo=svg2rlg(str(OUT/'chladni-simulator/exp-research.svg'))

c=canvas.Canvas(str(PATH),pagesize=(W,H));c.setTitle('CHLADNI / 08 - Rev.05 Desain dan Wiring');c.setAuthor('House of EXP')
def rect(x,y,w,h,fill=BG,stroke=None,r=0):
 c.setFillColor(HexColor(fill));c.setLineWidth(.7)
 if stroke:c.setStrokeColor(HexColor(stroke))
 (c.roundRect(x,H-y-h,w,h,r,fill=1,stroke=bool(stroke)) if r else c.rect(x,H-y-h,w,h,fill=1,stroke=bool(stroke)))
def txt(t,x,y,size=10,font='Sans',color=INK,align='left'):
 c.setFillColor(HexColor(color));c.setFont(font,size);fn={'left':c.drawString,'right':c.drawRightString,'center':c.drawCentredString}[align];fn(x,H-y-size*.82,str(t))
def para(t,x,y,w,size=10,maxh=80,color=INK):
 p=Paragraph(t,ParagraphStyle('p',fontName='Sans',fontSize=size,leading=size*1.35,textColor=HexColor(color)));_,h=p.wrap(w,900);assert h<=maxh,(t,h,maxh);p.drawOn(c,x,H-y-h)
def line(x,y,xx,yy,color=LINE,lw=.8):
 c.setStrokeColor(HexColor(color));c.setLineWidth(lw);c.line(x,H-y,xx,H-yy)
def circle(x,y,r,fill,stroke=None):
 c.setFillColor(HexColor(fill));c.setLineWidth(.7)
 if stroke:c.setStrokeColor(HexColor(stroke))
 c.circle(x,H-y,r,fill=1,stroke=bool(stroke))
def arrow(x,y,xx,yy):
 line(x,y,xx,yy,ACCENT,1.2);angle=math.atan2(yy-y,xx-x)
 for d in[-.45,.45]:line(xx,yy,xx-6*math.cos(angle+d),yy-6*math.sin(angle+d),ACCENT,1.2)
def page(n,title):
 rect(0,0,W,H);txt('CHLADNI / 08',36,23,10,'Bold');scale=68/logo.width;logo.scale(scale,scale);renderPDF.draw(logo,c,738,H-17-logo.height*scale);logo.scale(1/scale,1/scale);line(36,46,806,46);txt(title,36,67,27,'Bold');line(36,558,806,558);txt(f'{n:02d} / 06',806,569,8,'Mono',MUTED,'right')
def end():c.showPage()
def table(x,y,width,headers,rows,cw,rowh=25,size=9):
 rect(x,y,width,24,INK,r=3);xx=x
 for label,col in zip(headers,cw):txt(label,xx+8,y+8,8,'Bold','#FFFFFF');xx+=col
 for i,row in enumerate(rows):
  yy=y+24+i*rowh
  if i%2==0:rect(x,yy,width,rowh,'#EBEDE4')
  xx=x
  for value,col in zip(row,cw):
   if pdfmetrics.stringWidth(str(value),'Sans',size)>col-16:para(str(value),xx+8,yy+5,col-16,size,rowh-5)
   else:txt(value,xx+8,yy+(rowh-size)/2,size)
   xx+=col
  line(x,yy+rowh,x+width,yy+rowh,LINE,.5)
def matrix(x,y,w,h):
 rect(x,y,w,h,'#111111',r=3);glyphs={'1':['00100','01100','00100','00100','00100','00100','01110'],'2':['01110','10001','00001','00010','00100','01000','11111'],'8':['01110','10001','10001','01110','10001','10001','01110'],'.':['0','0','0','0','0','0','1'],'5':['11111','10000','10000','11110','00001','00001','11110']};lit=set();col=3
 for char in'128.5':
  for iy,row in enumerate(glyphs[char]):
   for ix,v in enumerate(row):
    if v=='1':lit.add((col+ix,iy))
  col+=len(glyphs[char][0])+1
 pitch=min((w-12)/32,(h-10)/8);sx=x+(w-32*pitch)/2;sy=y+(h-8*pitch)/2
 for ix in range(32):
  for iy in range(8):circle(sx+(ix+.5)*pitch,sy+(iy+.5)*pitch,pitch*.25,'#D7A25C' if(ix,iy)in lit else '#35402F')
def service(x,y,w=410,h=220):
 rect(x,y,w,h,'#171717','#343434',6);matrix(x+20,y+16,w-40,57)
 for i,label in enumerate(['FREQ','LEVEL','WAVE','FOLD','HARM.']):
  cx=x+42+i*(w-84)/4;cy=y+116
  circle(cx,cy,21,'#090909','#393939');line(cx,cy-16,cx,cy-7,'#E4DFC6',1.5);txt(label,cx,cy+32,8,'Mono','#D1D1D1',align='center')
 for xx,label in[(x+w-133,'SAVE'),(x+w-67,'MUTE')]:rect(xx,y+h-41,56,23,'#101010','#393939',3);txt(label,xx+28,y+h-34,8,'Mono','#CDCDCD',align='center')
 txt('RUN / SERVICE',x+20,y+h-33,8,'Mono','#969696')
def visitor(x,y,s=1):
 rect(x,y,232*s,125*s,'#171717','#343434',5)
 for i in range(8):
  xx=x+(18+i%4*51)*s;yy=y+(15+i//4*51)*s;rect(xx,yy,43*s,43*s,'#0D0D0D','#303030',3);txt(f'{i+1:02d}',xx+7*s,yy+7*s,8*s,'Mono','#CBCBCB');circle(xx+35*s,yy+8*s,1.5*s,'#A6B496')
# 1
page(1,'Instalasi')
img=ImageReader(str(OUT/'Chladni-08-Visual-Rev03.png'));iw,ih=img.getSize();scale=min(447/iw,300/ih);c.drawImage(img,36+(447-iw*scale)/2,H-122-ih*scale,iw*scale,ih*scale,mask='auto')
for y,head,value in[(131,'PELAT','80 x 80 cm'),(211,'KACA','Diameter 50 cm'),(291,'CELAH','30 mm')]:txt(head,536,y,9,'Mono',MUTED);txt(value,536,y+20,23,'Bold')
line(36,431,806,431)
# Section: square plate separated from supported glass
rect(51,475,74,12,'#A7B49E');rect(328,475,74,12,'#A7B49E');rect(113,470,226,5,'#BCD1C4');rect(69,509,315,5,'#34452F');rect(211,514,46,22,'#A7B49E')
line(416,475,416,509,MUTED);line(410,475,422,475,MUTED);line(410,509,422,509,MUTED);txt('30 mm',427,486,8,'Mono',MUTED)
para('<b>Beban:</b> sepatu > kaca > casing.<br/>Pelat bergetar tanpa beban kaca dan sepatu.',536,447,270,11,55)
para('Tinggi casing acuan 35 cm. Tebal kaca, dudukan, dan celah final ditetapkan setelah uji rakitan.',536,507,270,9,40,MUTED)
end()
# 2
page(2,'Panel dan operasi')
service(36,119,442,222);visitor(546,120,1.1)
table(36,365,442,['KONTROL','FUNGSI'],[('FREQ','Frekuensi; push: langkah 1 / 0,1 Hz'),('LEVEL','Level keluaran 0-100%'),('WAVE','Sine / Saw / Triangle'),('FOLD','Jumlah folding 0-100%'),('HARMONICS','Odd / Even / Odd + even')],[115,327],rowh=27,size=9.5)
for y,n,head,body in[(290,'01','SERVICE','Atur lima knob.'),(359,'02','SAVE','Tekan SAVE, lalu pad tujuan.'),(428,'03','RUN','Tutup panel servis. Pengunjung memilih pad.')]:
 txt(n,546,y,18,'Mono',ACCENT);txt(head,578,y,11,'Bold');para(body,578,y+20,212,10,40)
para('Preset menyimpan frekuensi, level, wave, fold, dan harmonik. Satu pad aktif. Boot perangkat: RUN + MUTE.',546,501,244,9,45,MUTED)
end()
# 3 ESP
page(3,'ESP32-S3 DevKitC-1 N16R8')
bx,by,bw,bh=318,115,206,351;rect(bx,by,bw,bh,'#2B4030',None,6);rect(bx+29,by+15,148,62,'#B6BDAA',None,3);txt('ESP32-S3',421,145,15,'Bold',align='center');txt('N16R8',421,169,10,'Mono',align='center')
txt('J1',bx+19,by-15,9,'Bold');txt('J3',bx+bw-18,by-15,9,'Bold',align='right')
for i,(l,r)in enumerate(zip(LEFT,RIGHT)):
 yy=by+94+i*10.7
 for left,name in[(True,l),(False,r)]:
  p=BY_GPIO.get(int(name))if name.isdigit()else None;color=COLORS[p[4]]if p else '#8B9482';cx=bx+12 if left else bx+bw-12;circle(cx,yy,3,color)
  txt('GPIO'+name if name.isdigit()else name,bx+23 if left else bx+bw-23,yy-4,7,'Mono','#F1F2E8','left'if left else'right')
  desc=p[3]if p else name if name in['5V','3V3','GND']else'-';txt(f'{i+1:02d}  {desc}',52 if left else 555,yy-4,7.7,'Sans',color)
rect(bx+20,by+325,56,17,'#B6BDAA',None,3);rect(bx+130,by+325,56,17,'#B6BDAA',None,3);txt('USB-UART',bx+48,by+330,6,'Mono',align='center');txt('USB',bx+158,by+330,6,'Mono',align='center')
para('<b>Pad 1-8:</b> GPIO 1, 2, 4, 5, 6, 7, 8, 9.<br/><b>FREQ:</b> A=10, B=11, push=12. <b>LEVEL:</b> A=13, B=14.<br/><b>SAVE:</b> 15. <b>MUTE:</b> 16.',36,488,465,10,52)
para('Input: pull-up 10k ke 3V3; kontak ke GND. Encoder: common ke GND. Tactile 4-pin: cek pasangan kontak sebelum solder.',548,488,258,10,52)
end()
# 4
page(4,'Audio dan daya')
table(36,117,368,['ESP32 / CATU','GY-PCM5102'],[('GPIO18 / J1-11','BCK'),('GPIO47 / J3-17','DIN'),('GPIO21 / J3-18','LCK / LRCK'),('GND','SCK, FMT, DEMP, FLT, GND'),('3V3','XSMT'),('5 V*','VIN / VCC')],[178,190],rowh=28,size=10)
rect(448,117,358,81,'#EBEDE4',LINE,5);txt('GY-PCM5102 > YS-502MT AUX',464,133,12,'Bold');para('TRS 3,5 mm: Tip=L, Ring=R, Sleeve=AGND.<br/>Kirim sinyal yang sama ke L dan R.',464,156,326,10,36)
rect(448,216,358,74,'#EBEDE4',LINE,5);txt('YS-502MT > SHAKER',464,232,12,'Bold');para('Gunakan satu kanal, misalnya L+ dan L-.<br/>L- / R- bukan GND. Jangan paralelkan kanal.',464,255,326,10,35)
para('*5 V hanya untuk modul GY dengan regulator 3,3 V. Ikuti nama pin PCB. Tegangan adaptor dan impedansi shaker mengikuti label YS-502MT.',36,334,770,10,40)
# power branches
rect(36,405,150,55,INK,None,4);txt('ADAPTOR 5 V / 3 A',111,425,11,'Bold','#FFFFFF','center')
for x,title,body in[(242,'ESP32','5V via J_PWR'),(440,'MAX + AHCT','Rail 5 V'),(638,'DAC','VIN 5 V*')]:
 rect(x,405,168,55,'#FFFFFF',LINE,4);txt(title,x+84,417,11,'Bold',align='center');txt(body,x+84,438,9,'Mono',MUTED,'center')
arrow(186,432,242,432);line(216,384,722,384,ACCENT);line(216,384,216,432,ACCENT);line(524,384,524,405,ACCENT);line(722,384,722,405,ACCENT)
para('<b>3V3 ESP32:</b> MCP23017, pull-up, dan LED. <b>Amplifier:</b> catu terpisah.<br/><b>USB programming:</b> lepas J_PWR; periferal tetap memakai rail 5 V dan ground bersama.<br/><b>Audio:</b> I2S stereo 48 kHz, slot 32-bit / 64 Fs. Mulai dari MUTE dan level rendah.',36,489,770,10,55)
end()
# 5
page(5,'Display dan ekspander')
table(36,117,368,['SN74AHCT125N / PDIP-14','SAMBUNGAN'],[('2 / 1A','GPIO39'),('3 / 1Y','MAX7219 DIN'),('5 / 2A','GPIO40'),('6 / 2Y','MAX7219 CLK'),('9 / 3A','GPIO41'),('8 / 3Y','MAX7219 CS / LOAD'),('14 / VCC','5 V'),('7, 1, 4, 10','GND'),('13 / OE4; 12 / A4; 11 / Y4','5 V; GND; kosong')],[188,180],rowh=28,size=9.4)
table(438,117,368,['MCP23017-E/SP / SPDIP-28','SAMBUNGAN'],[('9 / VDD','3V3'),('10 / VSS','GND'),('12 / SCL','GPIO17 / J1-10'),('13 / SDA','GPIO42 / J3-6'),('15, 16, 17 / A0-A2','GND: alamat 0x20'),('18 / RESET','Pull-up 10k ke 3V3'),('1-8 / GPB0-GPB7','Katode LED pad 1-8'),('11, 14, 19, 20','Kosong'),('28 / GPA7','Tanpa kabel; output LOW')],[188,180],rowh=28,size=9.4)
para('<b>MAX7219 quad 32 x 8:</b> VCC 5 V, GND bersama; gunakan sisi IN.<br/><b>Decoupling:</b> AHCT 100 nF; matrix 100 uF + 100 nF.<br/>GPIO39/40: pull-down 10k. GPIO41: pull-up 10k ke 3V3.',36,423,368,10,70)
para('<b>LED:</b> 3V3 > 1k > anode LED; katode ke GPB. LOW = ON.<br/><b>I2C:</b> SCL dan SDA masing-masing pull-up 4,7k ke 3V3.<br/><b>MCP:</b> 100 nF antara VDD-VSS. Inisialisasi GPB HIGH.',438,423,368,10,70)
# small level diagram
for x,label in[(36,'ESP32 / 3,3 V'),(296,'AHCT / 5 V'),(556,'MAX7219')]:rect(x,513,250,27,'#EBEDE4',None,3);txt(label,x+125,522,9,'Mono',align='center')
arrow(286,526,296,526);arrow(546,526,556,526)
end()
# 6
page(6,'WAVE, FOLD, dan HARMONICS')
table(36,117,770,['MCP23017','PIN FISIK','KONTROL','POSISI / FUNGSI'],[
('GPA0','21','RUN / SERVICE','HIGH=RUN; LOW=SERVICE'),('GPA1','22','WAVE kontak 1','LOW=SINE'),('GPA2','23','WAVE kontak 3','LOW=TRIANGLE; keduanya HIGH=SAW'),('GPA3','24','HARMONICS kontak 1','LOW=ODD'),('GPA4','25','FOLD encoder A','A ke GPA4; common ke GND'),('GPA5','26','FOLD encoder B','B ke GPA5; push tidak dipakai'),('GPA6','27','HARMONICS kontak 3','LOW=ODD+EVEN; keduanya HIGH=EVEN')],[105,88,212,365],rowh=27,size=9.5)
para('<b>WAVE dan HARMONICS:</b> selektor rotary SP3T, 3 posisi tetap. COM ke GND. Kontak tengah tidak diberi kabel. Setiap jalur input memakai pull-up 10k ke 3V3. Dua input LOW bersamaan: tahan pilihan sah terakhir.',36,349,770,10,44)
for x,title,left,right in[(36,'WAVE','GPA1 / SINE','GPA2 / TRI'),(438,'HARMONICS','GPA3 / ODD','GPA6 / O+E')]:
 rect(x,410,368,78,'#EBEDE4',LINE,4);txt(title,x+16,422,10,'Bold');txt('COM > GND',x+352,422,8,'Mono',MUTED,'right')
 for j,(label,wire)in enumerate([(left,True),('TENGAH / NC',False),(right,True)]):
  xx=x+55+j*126;circle(xx,459,3,INK if wire else'#B7C2AA');txt(label,xx,474,7.5,'Mono',MUTED,'center')
para('<b>SERVICE:</b> I2C 400 kHz, polling encoder 1 ms. <b>RUN:</b> hanya recall pad.<br/><b>Uji rakitan:</b> short/polaritas > input/display > DAC/AUX > shaker > sweep/preset.',36,505,770,9.5,30)
sources=[('ESP32','https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html'),('PCM5102A','https://www.ti.com/lit/ds/symlink/pcm5102a.pdf'),('AHCT125','https://www.ti.com/lit/ds/symlink/sn74ahct125.pdf'),('MCP23017','https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-MCP23S17-16-Bit-IO-Expander-with-Serial-Interface-DS20001952.pdf'),('MAX7219','https://www.analog.com/media/en/technical-documentation/data-sheets/MAX7219-MAX7221.pdf')]
for i,(label,url)in enumerate(sources):
 xx=36+i*130;txt(label,xx,542,7.5,'Mono',MUTED);c.linkURL(url,(xx,H-551,xx+110,H-540),relative=0,thickness=0)
end();c.save()
# Concise solder reference, matching the five-control revision.
buf=io.StringIO();cw=csv.writer(buf);cw.writerow(['source','pin_or_header','destination','notes'])
for key,gpio,header,dest,group in PINS:cw.writerow(['ESP32-S3',f'GPIO{gpio} / {header}',dest,group])
for pin,port,dest in[(21,'GPA0','RUN/SERVICE'),(22,'GPA1','WAVE terminal 1: SINE'),(23,'GPA2','WAVE terminal 3: TRIANGLE'),(24,'GPA3','HARMONICS terminal 1: ODD'),(25,'GPA4','FOLD A'),(26,'GPA5','FOLD B'),(27,'GPA6','HARMONICS terminal 3: ODD+EVEN')]:cw.writerow(['MCP23017-E/SP',f'{pin} / {port}',dest,'10k to 3V3; COM/contact to GND'])
cw.writerow(['SP3T WAVE','terminal 2','NC','both HIGH selects SAW'])
cw.writerow(['SP3T HARMONICS','terminal 2','NC','both HIGH selects EVEN'])
(OUT/'Chladni-08-Rev05-Pinout.csv').write_text(buf.getvalue())
print(PATH)
