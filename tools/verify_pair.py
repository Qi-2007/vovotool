#!/usr/bin/env python3
"""Side-by-side normalized instruction comparison for a CN/OS function pair."""
import re, sys
from difflib import SequenceMatcher
from capstone import CS_ARCH_ARM64, CS_MODE_ARM, Cs

MD = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
IMM = re.compile(r'#-?(?:0x[0-9a-fA-F]+|\d+)')
HEX = re.compile(r'\b0x[0-9a-fA-F]+\b')

def norm(ins):
    op = IMM.sub('#I', ins.op_str)
    op = HEX.sub('A', op)
    return ins.mnemonic + ' ' + op

def main():
    cnp, osp, cn_rva, os_rva = sys.argv[1], sys.argv[2], int(sys.argv[3],16), int(sys.argv[4],16)
    n = int(sys.argv[5], 16) if len(sys.argv) > 5 else 80
    cnd = open(cnp,'rb').read()[cn_rva:cn_rva+n*4]
    osd = open(osp,'rb').read()[os_rva:os_rva+n*4]
    ci = list(MD.disasm(cnd, cn_rva))
    oi = list(MD.disasm(osd, os_rva))
    cn = [norm(i) for i in ci]
    os_ = [norm(i) for i in oi]
    # find first divergence
    diff = next((k for k in range(min(len(cn),len(os_))) if cn[k]!=os_[k]), min(len(cn),len(os_)))
    print(f'CN 0x{cn_rva:x} ({len(cn)} insns) vs OS 0x{os_rva:x} ({len(os_)} insns); first diff at #{diff}')
    print('similarity(first diff..):', round(SequenceMatcher(None,cn,os_,autojunk=False).ratio(),4))
    print(' %-4s %-42s | %-4s %-42s' % ('#','CN','#','OS'))
    for k in range(min(max(diff+12, 32), max(len(cn),len(os_)))):
        a = cn[k] if k < len(cn) else ''
        b = os_[k] if k < len(os_) else ''
        flag = ' ' if a==b else '!'
        print(f'{flag}{k:<3} {a:<42} | {k:<3} {b}')

main()
