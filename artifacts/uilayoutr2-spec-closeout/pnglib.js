// Minimal PNG decode/encode + 5x7 bitmap text, so overlays can be produced with
// no third-party image tool on the evidence machine.
const zlib = require('zlib');

function decode(buf) {
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error('not a PNG');
  let off = 8, ihdr = null, idat = [], pal = null, trns = null;
  while (off < buf.length) {
    const len = buf.readUInt32BE(off);
    const type = buf.toString('ascii', off + 4, off + 8);
    const data = buf.slice(off + 8, off + 8 + len);
    if (type === 'IHDR') {
      ihdr = { w: data.readUInt32BE(0), h: data.readUInt32BE(4), depth: data[8],
        color: data[9], interlace: data[12] };
    } else if (type === 'IDAT') idat.push(data);
    else if (type === 'PLTE') pal = data;
    else if (type === 'tRNS') trns = data;
    else if (type === 'IEND') break;
    off += 12 + len;
  }
  if (ihdr.depth !== 8) throw new Error('unsupported bit depth ' + ihdr.depth);
  if (ihdr.interlace !== 0) throw new Error('interlaced PNG unsupported');
  const chan = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[ihdr.color];
  if (!chan) throw new Error('unsupported colour type ' + ihdr.color);
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const w = ihdr.w, h = ihdr.h, bpp = chan, stride = w * bpp;
  const out = Buffer.alloc(stride * h);
  let p = 0;
  for (let y = 0; y < h; y++) {
    const f = raw[p++];
    const line = raw.slice(p, p + stride); p += stride;
    const cur = out.slice(y * stride, (y + 1) * stride);
    const prev = y ? out.slice((y - 1) * stride, y * stride) : null;
    for (let i = 0; i < stride; i++) {
      const a = i >= bpp ? cur[i - bpp] : 0;
      const b = prev ? prev[i] : 0;
      const c = (prev && i >= bpp) ? prev[i - bpp] : 0;
      let v = line[i];
      if (f === 1) v += a;
      else if (f === 2) v += b;
      else if (f === 3) v += (a + b) >> 1;
      else if (f === 4) {
        const pp = a + b - c, pa = Math.abs(pp - a), pb = Math.abs(pp - b), pc = Math.abs(pp - c);
        v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
      }
      cur[i] = v & 0xff;
    }
  }
  // normalise to RGB
  const rgb = Buffer.alloc(w * h * 3);
  for (let i = 0; i < w * h; i++) {
    if (ihdr.color === 3) {
      const q = out[i] * 3; rgb[i * 3] = pal[q]; rgb[i * 3 + 1] = pal[q + 1]; rgb[i * 3 + 2] = pal[q + 2];
    } else if (chan === 1 || chan === 2) {
      const v = out[i * chan]; rgb[i * 3] = v; rgb[i * 3 + 1] = v; rgb[i * 3 + 2] = v;
    } else {
      rgb[i * 3] = out[i * chan]; rgb[i * 3 + 1] = out[i * chan + 1]; rgb[i * 3 + 2] = out[i * chan + 2];
    }
  }
  return { w, h, rgb };
}

function encode(img) {
  const { w, h, rgb } = img, stride = w * 3;
  const raw = Buffer.alloc((stride + 1) * h);
  for (let y = 0; y < h; y++) {
    raw[y * (stride + 1)] = 0;
    rgb.copy(raw, y * (stride + 1) + 1, y * stride, (y + 1) * stride);
  }
  const chunks = [Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a])];
  const chunk = (type, data) => {
    const b = Buffer.alloc(12 + data.length);
    b.writeUInt32BE(data.length, 0); b.write(type, 4, 'ascii');
    data.copy(b, 8); b.writeInt32BE(crc(Buffer.concat([Buffer.from(type, 'ascii'), data])), 8 + data.length);
    return b;
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  chunks.push(chunk('IHDR', ihdr));
  chunks.push(chunk('IDAT', zlib.deflateSync(raw, { level: 6 })));
  chunks.push(chunk('IEND', Buffer.alloc(0)));
  return Buffer.concat(chunks);
}

let CRCT = null;
function crc(buf) {
  if (!CRCT) {
    CRCT = new Int32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
      CRCT[n] = c;
    }
  }
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRCT[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) | 0;
}

// --- drawing ---
function px(img, x, y, col) {
  if (x < 0 || y < 0 || x >= img.w || y >= img.h) return;
  const i = (y * img.w + x) * 3;
  img.rgb[i] = col[0]; img.rgb[i + 1] = col[1]; img.rgb[i + 2] = col[2];
}
function fillRect(img, x, y, w, h, col, alpha) {
  for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) {
    if (i < 0 || j < 0 || i >= img.w || j >= img.h) continue;
    const k = (j * img.w + i) * 3;
    if (alpha === undefined || alpha >= 1) { img.rgb[k] = col[0]; img.rgb[k + 1] = col[1]; img.rgb[k + 2] = col[2]; }
    else for (let c = 0; c < 3; c++) img.rgb[k + c] = Math.round(img.rgb[k + c] * (1 - alpha) + col[c] * alpha);
  }
}
function strokeRect(img, x, y, w, h, col, t, dash) {
  for (let i = 0; i < w; i++) {
    if (dash && (Math.floor((x + i) / dash) % 2)) continue;
    for (let k = 0; k < t; k++) { px(img, x + i, y + k, col); px(img, x + i, y + h - 1 - k, col); }
  }
  for (let j = 0; j < h; j++) {
    if (dash && (Math.floor((y + j) / dash) % 2)) continue;
    for (let k = 0; k < t; k++) { px(img, x + k, y + j, col); px(img, x + w - 1 - k, y + j, col); }
  }
}

const FONT = {
  '0': [0x3E, 0x51, 0x49, 0x45, 0x3E], '1': [0x00, 0x42, 0x7F, 0x40, 0x00],
  '2': [0x42, 0x61, 0x51, 0x49, 0x46], '3': [0x21, 0x41, 0x45, 0x4B, 0x31],
  '4': [0x18, 0x14, 0x12, 0x7F, 0x10], '5': [0x27, 0x45, 0x45, 0x45, 0x39],
  '6': [0x3C, 0x4A, 0x49, 0x49, 0x30], '7': [0x01, 0x71, 0x09, 0x05, 0x03],
  '8': [0x36, 0x49, 0x49, 0x49, 0x36], '9': [0x06, 0x49, 0x49, 0x29, 0x1E],
  'A': [0x7E, 0x11, 0x11, 0x11, 0x7E], 'B': [0x7F, 0x49, 0x49, 0x49, 0x36],
  'C': [0x3E, 0x41, 0x41, 0x41, 0x22], 'D': [0x7F, 0x41, 0x41, 0x22, 0x1C],
  'E': [0x7F, 0x49, 0x49, 0x49, 0x41], 'F': [0x7F, 0x09, 0x09, 0x09, 0x01],
  'G': [0x3E, 0x41, 0x49, 0x49, 0x7A], 'H': [0x7F, 0x08, 0x08, 0x08, 0x7F],
  'I': [0x00, 0x41, 0x7F, 0x41, 0x00], 'J': [0x20, 0x40, 0x41, 0x3F, 0x01],
  'K': [0x7F, 0x08, 0x14, 0x22, 0x41], 'L': [0x7F, 0x40, 0x40, 0x40, 0x40],
  'M': [0x7F, 0x02, 0x0C, 0x02, 0x7F], 'N': [0x7F, 0x04, 0x08, 0x10, 0x7F],
  'O': [0x3E, 0x41, 0x41, 0x41, 0x3E], 'P': [0x7F, 0x09, 0x09, 0x09, 0x06],
  'Q': [0x3E, 0x41, 0x51, 0x21, 0x5E], 'R': [0x7F, 0x09, 0x19, 0x29, 0x46],
  'S': [0x46, 0x49, 0x49, 0x49, 0x31], 'T': [0x01, 0x01, 0x7F, 0x01, 0x01],
  'U': [0x3F, 0x40, 0x40, 0x40, 0x3F], 'V': [0x1F, 0x20, 0x40, 0x20, 0x1F],
  'W': [0x3F, 0x40, 0x38, 0x40, 0x3F], 'X': [0x63, 0x14, 0x08, 0x14, 0x63],
  'Y': [0x07, 0x08, 0x70, 0x08, 0x07], 'Z': [0x61, 0x51, 0x49, 0x45, 0x43],
  ' ': [0, 0, 0, 0, 0], '.': [0x00, 0x60, 0x60, 0x00, 0x00],
  ',': [0x00, 0x50, 0x30, 0x00, 0x00], ':': [0x00, 0x36, 0x36, 0x00, 0x00],
  '-': [0x08, 0x08, 0x08, 0x08, 0x08], '+': [0x08, 0x08, 0x3E, 0x08, 0x08],
  '=': [0x14, 0x14, 0x14, 0x14, 0x14], '/': [0x20, 0x10, 0x08, 0x04, 0x02],
  '(': [0x00, 0x1C, 0x22, 0x41, 0x00], ')': [0x00, 0x41, 0x22, 0x1C, 0x00],
  '_': [0x40, 0x40, 0x40, 0x40, 0x40], '#': [0x14, 0x7F, 0x14, 0x7F, 0x14],
  '<': [0x08, 0x14, 0x22, 0x41, 0x00], '>': [0x41, 0x22, 0x14, 0x08, 0x00],
  '%': [0x23, 0x13, 0x08, 0x64, 0x62], '!': [0x00, 0x00, 0x5F, 0x00, 0x00],
  '*': [0x14, 0x08, 0x3E, 0x08, 0x14], '[': [0x00, 0x7F, 0x41, 0x41, 0x00],
  ']': [0x00, 0x41, 0x41, 0x7F, 0x00],
};
function textWidth(s, sc) { return s.length * 6 * sc; }
function drawText(img, s, x, y, col, sc) {
  s = String(s).toUpperCase();
  let cx = x;
  for (const ch of s) {
    const g = FONT[ch] || FONT['#'];
    for (let c = 0; c < 5; c++) for (let r = 0; r < 7; r++) {
      if (g[c] & (1 << r)) fillRect(img, cx + c * sc, y + r * sc, sc, sc, col);
    }
    cx += 6 * sc;
  }
}
module.exports = { decode, encode, fillRect, strokeRect, drawText, textWidth };
