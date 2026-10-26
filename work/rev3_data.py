PINS=[
 ('PAD1',1,'J3-4','Pad 1','IN'),('PAD2',2,'J3-5','Pad 2','IN'),
 ('PAD3',4,'J1-4','Pad 3','IN'),('PAD4',5,'J1-5','Pad 4','IN'),
 ('PAD5',6,'J1-6','Pad 5','IN'),('PAD6',7,'J1-7','Pad 6','IN'),
 ('PAD7',8,'J1-12','Pad 7','IN'),('PAD8',9,'J1-15','Pad 8','IN'),
 ('FREQ_A',10,'J1-16','FREQ A','IN'),('FREQ_B',11,'J1-17','FREQ B','IN'),
 ('FREQ_SW',12,'J1-18','FREQ klik','IN'),('LEVEL_A',13,'J1-19','LEVEL A','IN'),
 ('LEVEL_B',14,'J1-20','LEVEL B','IN'),('SAVE',15,'J1-8','SAVE','IN'),
 ('MUTE',16,'J1-9','MUTE','IN'),('I2C_SCL',17,'J1-10','MCP SCL','I2C'),
 ('I2S_BCK',18,'J1-11','DAC BCK','AUDIO'),('I2S_LRCK',21,'J3-18','DAC LCK','AUDIO'),
 ('MATRIX_DIN',39,'J3-9','AHCT pin 2','MATRIX'),('MATRIX_CLK',40,'J3-8','AHCT pin 5','MATRIX'),
 ('MATRIX_CS',41,'J3-7','AHCT pin 9','MATRIX'),('I2C_SDA',42,'J3-6','MCP SDA','I2C'),
 ('I2S_DOUT',47,'J3-17','DAC DIN','AUDIO'),
]
assert len(PINS)==len({p[1] for p in PINS})==23
assert not ({p[1] for p in PINS}&{0,3,19,20,35,36,37,38,43,44,45,46,48})
BY_GPIO={p[1]:p for p in PINS}
COLORS={'IN':'#3D7767','I2C':'#866599','AUDIO':'#B56B2D','MATRIX':'#497C9B'}
LEFT=['3V3','3V3','RST','4','5','6','7','15','16','17','18','8','3','46','9','10','11','12','13','14','5V','GND']
RIGHT=['GND','43','44','1','2','42','41','40','39','38','37','36','35','0','45','48','47','21','20','19','GND','GND']
for key,gpio,header,_,_ in PINS:
 side,idx=header.split('-');assert (LEFT if side=='J1' else RIGHT)[int(idx)-1]==str(gpio)
