"""Vector annotations and physical header diagrams for the CP-100 wiring PDF.

Header order: Espressif DevKitC-1 user guide, J1/J3, top view, antenna up.
Photos remain original supplier references; no ownership/license transfer implied.
"""
from pathlib import Path
from reportlab.graphics.shapes import Drawing, Rect, String, Line, Circle, Image, PolyLine
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase.pdfmetrics import stringWidth

ROOT = Path(__file__).resolve().parents[1]
INK = HexColor('#1b2929'); GRAY = HexColor('#c7ceca'); TEAL = HexColor('#367e75')
POWER = HexColor('#b22f46'); GROUND = HexColor('#273438'); PULL = HexColor('#b66c14')
PAD = [HexColor(c) for c in ['#147b98','#8b4aa0','#358a56','#be651b']]
SIGNAL = [HexColor(c) for c in ['#b06412','#19788c','#8658a1']]
LEFT = ['3V3','3V3','RST','4','5','6','7','15','16','17','18','8','3','46','9','10','11','12','13','14','5V','GND']
RIGHT = ['GND','TX','RX','1','2','42','41','40','39','38','37','36','35','0','45','48','47','21','20','19','GND','GND']


def text(d,x,y,s,size=8,color=INK,anchor='start',bold=False):
    d.add(String(x,y,s,fontName='Helvetica-Bold' if bold else 'Helvetica',fontSize=size,fillColor=color,textAnchor=anchor))


def wire(d,points,color=INK,width=1.7,halo=True):
    flat=[c for p in points for c in p]
    if halo:d.add(PolyLine(flat,strokeColor=white,strokeWidth=width+2.5,fillColor=None))
    d.add(PolyLine(flat,strokeColor=color,strokeWidth=width,fillColor=None))


def dot(d,x,y,color=INK,r=2.6):d.add(Circle(x,y,r,fillColor=color,strokeColor=white,strokeWidth=.6))


def board(d,x,y,w=130,h=400,active=()):
    """22 rows per side, top-down, antenna at top and two USB at bottom."""
    d.add(Rect(x,y,w,h,rx=7,ry=7,fillColor=HexColor('#173f39'),strokeColor=INK))
    d.add(Rect(x+32,y+h-86,w-64,68,fillColor=HexColor('#d3d8d5'),strokeColor=GRAY))
    text(d,x+w/2,y+h-44,'ESP32-S3',10,anchor='middle',bold=True)
    text(d,x+w/2,y+h-60,'N16R8',9,anchor='middle')
    text(d,x+w/2,y+h-12,'ANTENA',7,white,'middle')
    text(d,x+w/2,y+83,'TOP VIEW',8,white,'middle')
    for ux,caption in [(x+24,'UART'),(x+w-49,'USB')]:
        d.add(Rect(ux,y-3,25,28,fillColor=HexColor('#d3d8d5'),strokeColor=INK,rx=3,ry=3))
        text(d,ux+12.5,y+32,caption,6.5,white,'middle')
    text(d,x+36,y+42,'RST',7,white)
    text(d,x+w-53,y+42,'BOOT',7,white)
    pins={}; top=y+h-30; pitch=(h-82)/21
    for side,labels,px,lx,anchor in [('J1',LEFT,x+6,x+13,'start'),('J3',RIGHT,x+w-6,x+w-13,'end')]:
        for i,label in enumerate(labels):
            py=top-i*pitch
            selected=(side,i+1) in active
            d.add(Circle(px,py,3,fillColor=HexColor('#e9bf62') if selected else HexColor('#a5b6ab'),strokeColor=INK,strokeWidth=.5))
            text(d,lx,py-2.4,label,6.7,white if selected else HexColor('#becdc7'),anchor,selected)
            pins[(side,i+1)]=(px,py)
    text(d,x-3,y+h+8,'J1 / KIRI',8,TEAL,bold=True)
    text(d,x+w+3,y+h+8,'J3 / KANAN',8,TEAL,'end',True)
    return pins


def photo_pin_map():
    d=Drawing(515,460)
    # Original 1600x1600 JPEG, scaled uniformly. Callouts point at header solder pads.
    x,y,size=55,25,405
    d.add(Image(x,y,size,size,str(ROOT/'work/component-photos/esp32.jpg')))
    def point(px,py):return (x+px/1600*size,y+(1600-py)/1600*size)
    left=[(220,'3V3 -> rail pull-up',PULL),(395,'GPIO4 -> PAD 3',PAD[2]),(455,'GPIO5 -> PAD 4',PAD[3]),(695,'GPIO16 -> MUTE*',TEAL),(820,'GPIO18 -> DAC BCK',SIGNAL[0]),(1420,'5V <- catu 5 V*',POWER),(1480,'GND -> rail GND',GROUND)]
    right=[(220,'GND -> DAC GND',GROUND),(400,'GPIO1 -> PAD 1',PAD[0]),(460,'GPIO2 -> PAD 2',PAD[1]),(1185,'GPIO47 -> DAC DIN',SIGNAL[2]),(1245,'GPIO21 -> DAC LCK',SIGNAL[1])]
    for items,px,tx,anchor in [(left,465,2,'start'),(right,1090,513,'end')]:
        for py,label,color in items:
            sx,sy=point(px,py);tw=stringWidth(label,'Helvetica-Bold',8);end=tx+tw+6 if anchor=='start' else tx-tw-6
            wire(d,[(sx,sy),(end,sy)],color,1.2)
            d.add(Circle(sx,sy,4,fillColor=None,strokeColor=color,strokeWidth=1.7))
            text(d,tx,sy+5,label,8,color,anchor,True)
    text(d,257,445,'ANTENA DI ATAS / LIHAT SISI KOMPONEN',9,TEAL,'middle',True)
    text(d,257,12,'DUA PORT USB DI BAWAH - gunakan port berlabel UART untuk flash',8,INK,'middle')
    return d


def pads_wiring():
    d=Drawing(515,470)
    active={('J1',1),('J1',4),('J1',5),('J1',9),('J1',22),('J3',4),('J3',5)}
    pins=board(d,193,55,130,360,active)
    # Each named rail is a single electrically common connection. Local branches are
    # kept short so the plug points remain readable and wires do not cover the PCB.
    def button(px,py,side,n,label,color,shift=0):
        node_x=111 if side=='left' else 404
        node_y=py+shift
        edge_x=84 if side=='left' else 431
        wire(d,[(px,py),(node_x,node_y),(edge_x,node_y)],color)
        dot(d,node_x,node_y,color)
        # 10k resistor from the signal junction to a labelled 3V3 rail.
        wire(d,[(node_x,node_y),(node_x,node_y+11)],PULL,1.2)
        d.add(Rect(node_x-3,node_y+11,6,17,fillColor=white,strokeColor=PULL))
        wire(d,[(node_x,node_y+28),(node_x,node_y+35)],PULL,1.2)
        text(d,node_x,node_y+39,'3V3',7,PULL,'middle',True)
        text(d,node_x+7,node_y+18,'10k',7,PULL)
        bx=44 if side=='left' else 431
        # Symbol shows NO contacts; signal contact and GND contact are separate.
        yy=node_y
        d.add(Rect(bx-8,yy-22,56,38,fillColor=HexColor('#f0eee7'),strokeColor=GRAY,rx=4,ry=4))
        ax=bx+40 if side=='left' else bx
        gx=bx if side=='left' else bx+40
        dot(d,ax,yy,color,2);dot(d,gx,yy,GROUND,2)
        d.add(Line(min(ax,gx)+2,yy+2,max(ax,gx)-4,yy+9,strokeColor=INK,strokeWidth=1))
        wire(d,[(gx,yy),(gx,yy-28)],GROUND,1.2)
        text(d,gx,yy-39,'GND',7,GROUND,'middle',True)
        text(d,bx+20,yy-16,label,7.7,color,'middle',True)
    for side,row,n,label,color,shift in [('right',4,1,'PAD 1',PAD[0],0),('right',5,2,'PAD 2',PAD[1],-61),('left',4,3,'PAD 3',PAD[2],0),('left',5,4,'PAD 4',PAD[3],-61),('left',9,5,'MUTE*',TEAL,-90)]:
        px,py=pins[('J1' if side=='left' else 'J3',row)]
        button(px,py,side,n,label,color,shift)
    # Explicit rail origin on the physical header, matching repeated rail labels.
    px,py=pins[('J1',1)];wire(d,[(px,py),(161,py),(161,444)],PULL,1.4);text(d,153,449,'3V3',8,PULL,'middle',True)
    px,py=pins[('J1',22)];wire(d,[(px,py),(151,py),(151,39)],GROUND,1.4);text(d,151,26,'GND',8,GROUND,'middle',True)
    text(d,257,4,'Titik 3V3 saling terhubung. Titik GND saling terhubung. * MUTE opsional.',8,INK,'middle')
    return d


def dac_wiring():
    d=Drawing(515,495)
    active={('J1',11),('J1',21),('J3',1),('J3',17),('J3',18)}
    pins=board(d,30,125,130,330,active)
    # Actual module photo is alongside a named terminal connector, whose order
    # matches the SCK/BCK/DIN/LRCK/GND/VIN row visible on this reference module.
    d.add(Image(300,280,205,205,str(ROOT/'work/component-photos/dac.jpg')))
    text(d,402,283,'Foto referensi GY-PCM5102 [4]',7.5,INK,'middle')
    d.add(Rect(330,86,172,179,fillColor=HexColor('#eee9f2'),strokeColor=HexColor('#866e9b'),rx=5,ry=5))
    text(d,416,245,'PCM5102',11,INK,'middle',True)
    text(d,416,230,'COCOKKAN NAMA PIN',7,TEAL,'middle',True)
    dest={}
    for name,yy in [('SCK',211),('BCK',192),('DIN',173),('LRCK / LCK',154),('GND',135),('VIN*',116)]:
        d.add(Circle(330,yy,3,fillColor=white,strokeColor=INK))
        text(d,342,yy-3,name,9,INK,bold=True);dest[name]=(330,yy)
    # Start from the real header location; only the named module connector is schematic.
    px,py=pins[('J1',11)];wire(d,[(px,py),(10,py),(10,480),(218,480),(218,192),dest['BCK']],SIGNAL[0]);dot(d,px,py,SIGNAL[0])
    text(d,219,485,'GPIO18 -> BCK',8,SIGNAL[0],bold=True)
    px,py=pins[('J3',17)];wire(d,[(px,py),(242,py),(242,173),dest['DIN']],SIGNAL[2]);dot(d,px,py,SIGNAL[2])
    text(d,246,179,'47 -> DIN',7.5,SIGNAL[2],bold=True)
    px,py=pins[('J3',18)];wire(d,[(px,py),(259,py),(259,154),dest['LRCK / LCK']],SIGNAL[1]);dot(d,px,py,SIGNAL[1])
    text(d,263,145,'21 -> LCK',7.5,SIGNAL[1],bold=True)
    px,py=pins[('J3',1)];wire(d,[(px,py),(200,py),(200,135),dest['GND']],GROUND,1.5);dot(d,px,py,GROUND)
    wire(d,[dest['SCK'],(313,211),(313,135),dest['GND']],GROUND,1.3);dot(d,313,135,GROUND)
    text(d,207,439,'GND bersama',7,GROUND,bold=True)
    # External 5V supply: show both positive and ground terminals explicitly.
    d.add(Rect(150,24,110,44,fillColor=HexColor('#f0eee7'),strokeColor=GRAY,rx=4,ry=4))
    text(d,205,36,'CATU 5 V TERATUR',7.2,INK,'middle',True)
    text(d,175,54,'+',9,POWER,'middle',True)
    text(d,235,54,'-',9,GROUND,'middle',True)
    wire(d,[(175,68),(175,80),(20,80),(20,pins[('J1',21)][1]),pins[('J1',21)]],POWER,1.8)
    wire(d,[(175,80),(285,80),(285,116),dest['VIN*']],POWER,1.8)
    wire(d,[(235,68),(235,103),(298,103),(298,135),(313,135)],GROUND,1.5)
    dot(d,175,68,POWER);dot(d,175,80,POWER);dot(d,235,68,GROUND)
    text(d,206,87,'+5 V',7.5,POWER,bold=True)
    text(d,248,109,'GND catu',7,GROUND,bold=True)
    text(d,416,101,'VIN* hanya breakout 5 V ber-regulator',6.7,POWER,'middle')
    text(d,257,9,'Mode daya eksternal: USB DILEPAS. GND catu ikut GND bersama.',8,POWER,'middle',True)
    return d
