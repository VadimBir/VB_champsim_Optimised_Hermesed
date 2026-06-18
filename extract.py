import pdfplumber, sys
pdf = pdfplumber.open('/home/cc/champsim_VB/ICCD26_MARS (4).pdf')
print(f"TOTAL PAGES: {len(pdf.pages)}")
start = int(sys.argv[1]) if len(sys.argv) > 1 else 1
end = int(sys.argv[2]) if len(sys.argv) > 2 else len(pdf.pages)
for i in range(start-1, min(end, len(pdf.pages))):
    print(f"\n=== PAGE {i+1} ===")
    t = pdf.pages[i].extract_text()
    print(t if t else "[NO TEXT]")
