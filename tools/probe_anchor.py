#!/usr/bin/env python3
"""Anchor vote for a single CN function, restricted to same-section OS candidates."""
import struct, sys
from collections import Counter
from elftools.elf.elffile import ELFFile

def sect(elf, addr):
    for s in elf.iter_sections():
        if s['sh_flags'] & 0x4 and s['sh_addr'] <= addr < s['sh_addr'] + s['sh_size']:
            return s.name
    return None

def run(cnp, osp, rva, winsize=512, seedlen=8):
    cndata = open(cnp, 'rb').read()
    osdata = open(osp, 'rb').read()
    with open(cnp, 'rb') as f:
        celf = ELFFile(f)
        csection = sect(celf, rva)
        csec = next(s for s in celf.iter_sections() if s.name == csection)
        clo, chi = csec['sh_addr'], csec['sh_addr'] + csec['sh_size']
    with open(osp, 'rb') as f:
        oelf = ELFFile(f)
        osec = next(s for s in oelf.iter_sections() if s.name == csection)
        olo, ohi = osec['sh_addr'], osec['sh_addr'] + osec['sh_size']
    body = cndata[rva:rva+winsize]
    votes = Counter()
    for off in range(0, len(body)-seedlen+1, 4):
        seed = body[off:off+seedlen]
        pos = olo
        while True:
            hit = osdata.find(seed, pos, ohi)
            if hit < 0: break
            votes[hit - off] += 1
            pos = hit + 1
    print(f'CN 0x{rva:x} section={csection}; OS section range 0x{olo:x}-0x{ohi:x}')
    print('top anchors (os_func_guess, votes, delta):')
    for cand, v in votes.most_common(12):
        print('  0x%x votes=%d delta=%+d' % (cand, v, cand - rva))
    print('total distinct anchors', len(votes))

if __name__ == '__main__':
    run(sys.argv[1], sys.argv[2], int(sys.argv[3], 16))
