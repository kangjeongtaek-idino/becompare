import zipfile, sys
out = sys.argv[1]
def make(path, rows, shared_extra=''):
    strings = []
    def si(t):
        if t not in strings: strings.append(t)
        return strings.index(t)
    sheet_rows = []
    for r, row in enumerate(rows, 1):
        cells = []
        for c, v in enumerate(row):
            if v is None: continue
            ref = chr(65 + c) + str(r)
            if isinstance(v, (int, float)):
                cells.append('<c r="%s"><v>%s</v></c>' % (ref, v))
            else:
                cells.append('<c r="%s" t="s"><v>%d</v></c>' % (ref, si(v)))
        sheet_rows.append('<row r="%d">%s</row>' % (r, ''.join(cells)))
    sst = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count="%d" uniqueCount="%d">%s</sst>' % (
        len(strings), len(strings), ''.join('<si><t xml:space="preserve">%s</t></si>' % s.replace('&', '&amp;').replace('<', '&lt;') for s in strings))
    sheet = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>%s</sheetData></worksheet>' % ''.join(sheet_rows)
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', '<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"/>')
        z.writestr('xl/sharedStrings.xml', sst)
        z.writestr('xl/worksheets/sheet1.xml', sheet)
make(out + '/x1.xlsx', [['이름', '수량', '메모'], ['apple', 10, 'a & b'], ['banana', 20, '한글 셀'], ['cherry', 30, None]])
make(out + '/x2.xlsx', [['이름', '수량', '메모'], ['apple', 10, 'a & b'], ['banana', 25, '한글 셀'], ['durian', 40, 'new']])
print('ok')
