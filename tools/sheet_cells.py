#!/usr/bin/env python3
"""sheet_cells.py - small, surgical edits to an .xlsx course workbook WITHOUT re-saving it through a spreadsheet
library (which would drop the cached formula results the course tools read). Edits the XML parts in place:

  rename  <old text> <new text>       every cell whose whole text is <old> (shared string), any sheet
  set     <sheet> <cell> <text|=num>  one cell: a new string (=12 for a number); a formula there is replaced

usage: python3 -I tools/sheet_cells.py <book.xlsx> <edits.txt>   (one edit per line, fields separated by TABs)
Everything else in the file - other cells, formulas and their cached values, styles - stays byte for byte.
"""
import re
import sys
import zipfile
from xml.sax.saxutils import escape


def load(path):
    with zipfile.ZipFile(path) as z:
        return [(i, z.read(i.filename)) for i in z.infolist()]


def sheet_part(parts, name):
    wb = dict((i.filename, d) for i, d in parts)["xl/workbook.xml"].decode("utf-8")
    rels = dict((i.filename, d) for i, d in parts)["xl/_rels/workbook.xml.rels"].decode("utf-8")
    m = re.search(r'<sheet [^>]*name="%s"[^>]*r:id="([^"]+)"' % re.escape(escape(name, {"'": "&apos;"})), wb) or \
        re.search(r'<sheet [^>]*name="%s"[^>]*r:id="([^"]+)"' % re.escape(escape(name)), wb)
    rid = m.group(1)
    t = re.search(r'<Relationship [^>]*Id="%s"[^>]*Target="([^"]+)"' % rid, rels) or \
        re.search(r'<Relationship [^>]*Target="([^"]+)"[^>]*Id="%s"' % rid, rels)
    target = t.group(1).lstrip("/")
    return target if target.startswith("xl/") else "xl/" + target


def main():
    book, edits = sys.argv[1], sys.argv[2]
    parts = load(book)
    data = {i.filename: d for i, d in parts}
    ss = data["xl/sharedStrings.xml"].decode("utf-8")
    items = re.findall(r"<si>.*?</si>", ss, flags=re.S)

    def text_of(si):
        return "".join(re.findall(r"<t[^>]*>(.*?)</t>", si, flags=re.S))

    def add_string(s):
        nonlocal ss, items
        si = "<si><t>%s</t></si>" % escape(s)
        ss = ss.replace("</sst>", si + "</sst>")
        items.append(si)
        return len(items) - 1

    for line in open(edits, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        f = line.split("\t")
        if f[0] == "rename":
            old, new = escape(f[1]), escape(f[2])
            hits = 0
            for k, si in enumerate(items):
                if text_of(si) in (old, old.replace("'", "&apos;")) and "<r>" not in si:
                    nsi = "<si><t>%s</t></si>" % new
                    ss = ss.replace(si, nsi, 1)
                    items[k] = nsi
                    hits += 1
            print("rename %r -> %r: %d shared string(s)" % (f[1], f[2], hits))
        elif f[0] == "set":
            part = sheet_part(parts, f[1])
            xml = data[part].decode("utf-8")
            cell = f[2]
            m = re.search(r'<c r="%s"([^>]*?)(/>|>.*?</c>)' % cell, xml, flags=re.S)
            if not m:  # an empty cell is not in the file: add it at the end of its row (columns after the last one)
                rown = re.match(r"[A-Z]+(\d+)", cell).group(1)
                rm = re.search(r'(<row r="%s"[^>]*>)(.*?)(</row>)' % rown, xml, flags=re.S)
                if not rm:
                    raise SystemExit("row %s of %s not found" % (rown, f[1]))
                last = re.findall(r'<c r="([A-Z]+)%s"' % rown, rm.group(2))
                col = re.match(r"([A-Z]+)", cell).group(1)
                if last and (len(last[-1]), last[-1]) >= (len(col), col):
                    raise SystemExit("cell %s is not after the row's last cell %s" % (cell, last[-1]))
                pos = rm.start(3)
                xml = xml[:pos] + '<c r="%s"/>' % cell + xml[pos:]
                m = re.search(r'<c r="%s"([^>]*?)(/>|>.*?</c>)' % cell, xml, flags=re.S)
            style = re.search(r'\ss="(\d+)"', m.group(1))
            s_attr = ' s="%s"' % style.group(1) if style else ""
            if f[3].startswith("="):
                new = '<c r="%s"%s><v>%s</v></c>' % (cell, s_attr, f[3][1:])
            else:
                idx = add_string(f[3])
                new = '<c r="%s"%s t="s"><v>%d</v></c>' % (cell, s_attr, idx)
            xml = xml[:m.start()] + new + xml[m.end():]
            data[part] = xml.encode("utf-8")
            print("set %s!%s = %r (was %s)" % (f[1], cell, f[3], m.group(0)[:90]))
    n = len(items)
    ss = re.sub(r'uniqueCount="\d+"', 'uniqueCount="%d"' % n, ss, count=1)
    data["xl/sharedStrings.xml"] = ss.encode("utf-8")
    # formulas that depended on a changed cell: let the spreadsheet recompute on open
    wbx = data["xl/workbook.xml"].decode("utf-8")
    if "<calcPr" in wbx and "fullCalcOnLoad" not in wbx:
        wbx = wbx.replace("<calcPr", '<calcPr fullCalcOnLoad="1"', 1)
        data["xl/workbook.xml"] = wbx.encode("utf-8")
    with zipfile.ZipFile(book, "w", zipfile.ZIP_DEFLATED) as z:
        for info, _ in parts:
            z.writestr(info, data[info.filename])
    print("wrote", book)


if __name__ == "__main__":
    main()
