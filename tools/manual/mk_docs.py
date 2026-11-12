import zipfile, zlib, sys, os
out = sys.argv[1]
os.makedirs(out, exist_ok=True)


def docx(path, paras):
    body = ''.join('<w:p><w:r><w:t xml:space="preserve">%s</w:t></w:r></w:p>' % p.replace('&', '&amp;') for p in paras)
    xml = '<?xml version="1.0" encoding="UTF-8"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body>%s</w:body></w:document>' % body
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', '<Types/>')
        z.writestr('word/document.xml', xml)


docx(out + '/a.docx', ['첫째 문단', 'Hello & World', '같은 줄'])
docx(out + '/b.docx', ['첫째 문단', 'Hello & Mars', '같은 줄'])


def rtf(path, lines):
    body = '\\par\n'.join(lines)
    open(path, 'w', encoding='ascii').write('{\\rtf1\\ansi{\\fonttbl{\\f0 Arial;}}{\\colortbl;\\red0\\green0\\blue0;}\\f0 ' + body + '\\par}')


rtf(out + '/a.rtf', ['Alpha line', 'Beta line \\u54620?\\u44544?', 'Gamma'])
rtf(out + '/b.rtf', ['Alpha line', 'Beta changed', 'Gamma'])


def pdf(path, lines):
    content = 'BT /F1 12 Tf 72 720 Td 14 TL\n' + ''.join('(%s) Tj T*\n' % l for l in lines) + 'ET'
    comp = zlib.compress(content.encode('latin-1'))
    objs = [b'<< /Type /Catalog /Pages 2 0 R >>', b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
            b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>',
            b'<< /Length %d /Filter /FlateDecode >>\nstream\n' % len(comp) + comp + b'\nendstream',
            b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
    data = b'%PDF-1.4\n'
    for i, o in enumerate(objs, 1):
        data += b'%d 0 obj\n' % i + o + b'\nendobj\n'
    data += b'trailer << /Root 1 0 R /Size 6 >>\n%%EOF\n'
    open(path, 'wb').write(data)


pdf(out + '/a.pdf', ['Invoice 001', 'Total (USD) 100', 'Thanks'])
pdf(out + '/b.pdf', ['Invoice 001', 'Total (USD) 120', 'Thanks'])
print('ok')
