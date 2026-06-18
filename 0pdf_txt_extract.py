import sys
import fitz  # PyMuPDF

def extract_two_col(page):
    blocks = page.get_text("blocks")  # (x0, y0, x1, y1, text, block_no, block_type)
    if not blocks:
        return ""
    page_mid = page.rect.width / 2
    left  = sorted([b for b in blocks if b[0] < page_mid], key=lambda b: (b[1], b[0]))
    right = sorted([b for b in blocks if b[0] >= page_mid], key=lambda b: (b[1], b[0]))
    return "".join(b[4] for b in left + right)

def flatten_paragraphs(text):
    lines = text.splitlines()
    out = []
    buf = ""
    for line in lines:
        stripped = line.strip()
        if not stripped:
            if buf:
                out.append(buf)
                buf = ""
            out.append("")
        else:
            if buf:
                buf += " " + stripped
            else:
                buf = stripped
            # end of paragraph: line ends with sentence-ending punctuation
            if stripped[-1] in ".?!:":
                out.append(buf)
                buf = ""
    if buf:
        out.append(buf)
    return "\n".join(out)

def main():
    if len(sys.argv) != 3:
        print("Usage: python extract_pdf.py <input.pdf> <output.txt>")
        sys.exit(1)

    input_pdf = sys.argv[1]
    output_txt = sys.argv[2]

    doc = fitz.open(input_pdf)
    full_text = ""
    for page in doc:
        full_text += extract_two_col(page) + "\n"
    doc.close()

    full_text = flatten_paragraphs(full_text)

    with open(output_txt, "w", encoding="utf-8") as f:
        f.write(full_text)

if __name__ == "__main__":
    main()
