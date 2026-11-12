import struct, os, sys
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
def ss(n): return bytes([(n>>21)&0x7f,(n>>14)&0x7f,(n>>7)&0x7f,n&0x7f])
def frame23(fid, payload): return fid + struct.pack('>I', len(payload)) + b'\0\0' + payload
def t(s, enc=3): return bytes([enc]) + s.encode('utf-8' if enc==3 else 'latin-1')
def t16(s): return b'\x01\xff\xfe' + s.encode('utf-16-le') + b'\0\0'
def mp3(path, title, artist, album, year, track, audioseed, v1=None, ver=3):
    frames = frame23(b'TIT2', t16(title)) + frame23(b'TPE1', t(artist, 0)) + frame23(b'TALB', t(album)) + \
             frame23(b'TYER', t(year, 0)) + frame23(b'TRCK', t(track, 0)) + \
             frame23(b'COMM', b'\0eng\0' + b'a comment') + frame23(b'APIC', b'\0image/jpeg\0\3\0' + b'\xff\xd8' + b'x'*100)
    tag = b'ID3' + bytes([3,0,0]) + ss(len(frames)) + frames
    audio = b''
    hdr = bytes([0xFF, 0xFB, 0x90, 0x64])      # MPEG1 Layer III 128kbps 44100 stereo
    for i in range(20): audio += hdr + bytes(((audioseed*7+i*13+j)&0xff) for j in range(413))
    data = tag + audio
    if v1:
        data += b'TAG' + v1[0].encode('latin-1').ljust(30,b'\0') + v1[1].encode('latin-1').ljust(30,b'\0') + v1[2].encode('latin-1').ljust(30,b'\0') + b'1999' + b'c'.ljust(28,b'\0') + b'\0' + bytes([3]) + bytes([17])
    open(path,'wb').write(data)
mp3(out+'/a.mp3','Hello World','Artist One','Album X','2020','1/10',1)
mp3(out+'/b.mp3','Hello Wörld','Artist One','Album Y','2021','1/10',2, v1=('v1title','v1artist','v1album'))
def flac(path, comments, total):
    si = struct.pack('>HH', 4096, 4096) + b'\0\0\0' + b'\0\0\0'
    rate=44100; ch=2; bps=16
    packed = (rate<<44)|((ch-1)<<41)|((bps-1)<<36)|total
    si += struct.pack('>Q', packed) + b'\0'*16
    vendor = b'bc'
    vc = struct.pack('<I', len(vendor)) + vendor + struct.pack('<I', len(comments))
    for c in comments:
        b = c.encode('utf-8'); vc += struct.pack('<I', len(b)) + b
    blocks = bytes([0, 0,0,34]) + si + bytes([0x84]) + struct.pack('>I', len(vc))[1:] + vc
    open(path,'wb').write(b'fLaC' + blocks + bytes((i*5)&0xff for i in range(3000)))
flac(out+'/a.flac',['TITLE=Song','ARTIST=Band','ALBUM=Rec','DATE=2019','TRACKNUMBER=3'], 44100*185)
flac(out+'/b.flac',['TITLE=Song','ARTIST=Band 2','ALBUM=Rec','date=2019','GENRE=Rock'], 44100*185)
print('ok')
