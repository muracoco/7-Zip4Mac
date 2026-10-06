#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Create disposable genuine format fixtures; never modify existing directories."""
import argparse
import base64
import binascii
import hashlib
import io
import json
import lzma
import pathlib
import shutil
import struct
import subprocess
import tarfile
import zlib
import gzip
import uuid
import ctypes

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('sevenzip', type=pathlib.Path)
parser.add_argument('output', type=pathlib.Path)
parser.add_argument('--sevenzip-source', type=pathlib.Path)
parser.add_argument('--fixture-tools', type=pathlib.Path)
args = parser.parse_args()
root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
repo = pathlib.Path(__file__).resolve().parents[1]
inventory = json.loads((repo/'resources/formats.json').read_text())
payload = 'ASCII / 日本語 / space in name\n'.encode()
work = root/'source'; work.mkdir(); (work/'payload.txt').write_bytes(payload)
cases = []; failures = []

def command(argv, cwd=work, input=None):
    result = subprocess.run([str(a) for a in argv], cwd=cwd, input=input, capture_output=True, timeout=120)
    if result.returncode:
        raise RuntimeError(f'{argv[0]}: exit {result.returncode}: '+(result.stdout+result.stderr).decode(errors='replace')[-2000:])
    return result.stdout

def record(fmt, extension, data, contents, provenance='Generated from original test payload; no third-party executable is run from the fixture', test=True, expected_test_exit=0, expected_extract_exit=0):
    folder = root/fmt/extension; folder.mkdir(parents=True, exist_ok=True)
    path = folder/('fixture.'+extension)
    if isinstance(data, pathlib.Path): shutil.copyfile(data, path)
    else: path.write_bytes(data)
    cases.append({'format': fmt, 'extension': extension, 'archive': str(path), 'expectedHashes': [hashlib.sha256(c).hexdigest() for c in contents], 'provenance': provenance, 'test': test, 'expectedTestExit': expected_test_exit, 'expectedExtractExit': expected_extract_exit})
    return path

def aliases(fmt, data, contents, extensions=None, **kwargs):
    entries = next(f for f in inventory['formats'] if f['name']==fmt)['extensions']
    for ext in (extensions if extensions is not None else entries): record(fmt, ext, data, contents, **kwargs)

def attempt(fmt, action):
    try: action()
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
        failures.append({'format': fmt, 'reason': str(error)})
        print('Fixture generation pending:',fmt,str(error)[:160], flush=True)

def create(fmt, extension, files=('payload.txt',)):
    path = root/('source.'+extension)
    command([args.sevenzip, 'a', '-t'+fmt, '-mx=1', '-sccUTF-8', '--', path, *files])
    return path

tar_path = root/'source.tar'
with tarfile.open(tar_path, 'w', format=tarfile.USTAR_FORMAT) as archive:
    member = tarfile.TarInfo('payload.txt'); member.size=len(payload); member.mtime=1700000000
    archive.addfile(member, io.BytesIO(payload))
aliases('tar', tar_path, [payload])
for fmt, extension in [('7z','7z'),('zip','zip'),('wim','wim')]:
    attempt(fmt, lambda f=fmt,e=extension: aliases(f, create(f,e), [payload], extensions=next(x for x in inventory['formats'] if x['name']==f)['extensions'] if f!='zip' else ['zip','zipx','jar','xpi','odt','ods','docx','xlsx','epub','ipa','apk','appx']))
# Container aliases test archive parsing; application-specific document/package
# semantics are outside 7-Zip's archive reader and are not asserted here.
for fmt, extension in [('gzip','gz'),('bzip2','bz2'),('xz','xz')]:
    def streams(f=fmt,e=extension):
        single=create(f,e)
        for ext in next(x for x in inventory['formats'] if x['name']==f)['extensions']:
            is_tar=ext in {'tgz','tpz','apk','tbz','tbz2','txz'}
            source=single
            if is_tar:
                source=root/('tar-stream-'+ext+'.'+e)
                command([args.sevenzip,'a','-t'+f,'-mx=1','--',source,tar_path])
            record(f,ext,source,[tar_path.read_bytes() if is_tar else payload])
    attempt(fmt,streams)
aliases('lzma', lzma.compress(payload, format=lzma.FORMAT_ALONE), [payload])
aliases('lzma86', b'\0'+lzma.compress(payload, format=lzma.FORMAT_ALONE), [payload])
# Zstandard uncompressed frame: this writes a stored container, not a codec.
def stored_zstd(data): return b'\x28\xb5\x2f\xfd\xa0'+struct.pack('<I',len(data))+((len(data)<<3)|1).to_bytes(3,'little')+data
aliases('zstd',stored_zstd(payload),[payload],extensions=['zst'])
aliases('zstd',stored_zstd(tar_path.read_bytes()),[tar_path.read_bytes()],extensions=['tzst'])
aliases('Base64',base64.encodebytes(payload),[payload])
# Intel HEX stores the same bytes at address 0; CRC is the standard line sum.
hex_lines=[]
for offset in range(0,len(payload),16):
    block=payload[offset:offset+16]; header=bytes([len(block),offset>>8,offset&255,0])+block
    hex_lines.append(':'+header.hex().upper()+f'{(-sum(header))&255:02X}')
aliases('IHex',('\n'.join(hex_lines)+'\n:00000001FF\n').encode(),[payload])
def ar_fixture():
    header=f'{"payload.txt/":<16}{1700000000:<12}{0:<6}{0:<6}{"100644":<8}{len(payload):<10}`\n'.encode()
    aliases('Ar',b'!<arch>\n'+header+payload+(b'\n' if len(payload)%2 else b''),[payload])
attempt('Ar',ar_fixture)
def cpio_fixture():
    data=command(['/usr/bin/cpio','-o','-H','newc'],input=b'payload.txt\n'); aliases('Cpio',data,[payload])
attempt('Cpio',cpio_fixture)
def xar_fixture():
    path=root/'source.xar'; command(['/usr/bin/xar','-cf',path,'payload.txt']); aliases('Xar',path,[payload])
attempt('Xar',xar_fixture)
def compress_fixture():
    aliases('Z',command(['/usr/bin/compress','-c','payload.txt']),[payload],extensions=['z'])
    aliases('Z',command(['/usr/bin/compress','-c',tar_path]),[tar_path.read_bytes()],extensions=['taz'])
attempt('Z',compress_fixture)
def uu_fixture(name):
    lines=(repo/'tests/fixtures'/name).read_bytes().splitlines(); start=next(i for i,l in enumerate(lines) if l.startswith(b'begin ')); out=[]
    for line in lines[start+1:]:
        if line==b'end':break
        out.append(binascii.a2b_uu(line))
    return b''.join(out)
known_file1=b'                          file 1 contents\nhello\nhello\nhello\n'
for fmt,name,contents in [('Cab','test_read_format_cab_1.cab.uu',[known_file1]),('Lzh','test_read_format_lha_header0.lzh.uu',[known_file1]),('Rar5','test_read_format_rar5_stored.rar.uu',[b'hello libarchive test suite!\n']),('Rar','test_read_format_rar.rar.uu',[b'test text document\r\n'])]:
    attempt(fmt,lambda f=fmt,n=name,c=contents: aliases(f,uu_fixture(n),c,extensions=next(x for x in inventory['formats'] if x['name']==f)['extensions'] if f=='Lzh' else [next(x for x in inventory['formats'] if x['name']==f)['extensions'][0]],provenance='libarchive v3.8.2 libarchive/test/'+n+'; BSD-2-Clause; licenses/libarchive-fixtures.txt'))
def rpm_fixture():
    data=uu_fixture('test_read_format_cpio_svr4_gzip_rpm.rpm.uu')
    # RPM enters GZip and exposes the original CPIO bytes.
    cpio=gzip.decompress(data[data.index(b'\x1f\x8b\x08'):])
    assert b'hello\n' in cpio
    aliases('Rpm',data,[cpio],provenance='libarchive v3.8.2; licenses/libarchive-fixtures.txt')
attempt('Rpm',rpm_fixture)
def hash_fixtures():
    supported={'sha256':'SHA256','sha512':'SHA512','sha384':'SHA384','sha1':'SHA1','sha2':'SHA256','sha3':'SHA3-256','sha':'SHA1','md5':'MD5','blake2sp':'BLAKE2sp','xxh64':'XXH64','crc32':'CRC32','crc64':'CRC64','sha3-256':'SHA3-256','asc':'SHA256'}
    for ext in next(f for f in inventory['formats'] if f['name']=='Hash')['extensions']:
        if ext in supported:
            source=root/('hash-source.'+ext)
            command([args.sevenzip,'a','-tHash','-mm='+supported[ext],'--',source,'payload.txt'])
            data=source.read_bytes(); exit_code=0
        elif ext=='cksum':
            data=command(['/usr/bin/cksum','payload.txt']); exit_code=2
        else:
            if ext in {'sha512-224','sha512-256'}:
                digest=command(['/usr/bin/shasum','-a',ext.removeprefix('sha').replace('-',''),'payload.txt']).split()[0].decode()
            else:digest=hashlib.new(ext.replace('-','_'),payload).hexdigest()
            data=f'{ext.upper()} (payload.txt) = {digest}\n'.encode(); exit_code=2
        path=record('Hash',ext,data,[],expected_test_exit=exit_code,expected_extract_exit=2,provenance='official 7zz / system shasum / Python hashlib / POSIX cksum; original file remains beside checksum')
        (path.parent/'payload.txt').write_bytes(payload)
attempt('Hash',hash_fixtures)
# Stored HTML Help / Help2 containers, based on official ChmIn.cpp layouts.
# These validate archive-reader aliases, not Microsoft viewer/index semantics.
def enc(value):
    groups=[value&127]; value >>= 7
    while value:groups.append((value&127)|128); value >>= 7
    return bytes(reversed(groups))
def help_entry(name, offset, size):
    data=name.encode(); return enc(len(data))+data+enc(0)+enc(offset)+enc(size)
name_list=struct.pack('<HHH',16,1,12)+'Uncompressed'.encode('utf-16le')+b'\0\0'
help_entries=help_entry('/payload.txt',len(name_list),len(payload))+help_entry('::DataSpace/NameList',0,len(name_list))
free=4096-20-len(help_entries); pmgl=struct.pack('<4sIIII',b'PMGL',free,0,0xffffffff,0xffffffff)+help_entries+bytes(free-2)+struct.pack('<H',2)
itsp=struct.pack('<4s12I16s4I',b'ITSP',1,84,10,4096,2,1,0xffffffff,0,0,0xffffffff,1,0x409,uuid.UUID('5d02926a-212e-11d0-9df9-00a0c922e6ec').bytes_le,84,0xffffffff,0xffffffff,0xffffffff)
directory=itsp+pmgl; content_offset=120+len(directory); length=content_offset+len(name_list)+len(payload)
chm_header=struct.pack('<4s5I16s16s5Q',b'ITSF',3,96,1,0,0x409,uuid.UUID('7c01fd10-7baa-11d0-9e0c-00a0c922e6ec').bytes_le,uuid.UUID('7c01fd11-7baa-11d0-9e0c-00a0c922e6ec').bytes_le,96,24,120,len(directory),content_offset)
aliases('Chm',chm_header+struct.pack('<IIQII',0x1fe,0,length,0,0)+directory+name_list+payload,[payload])
chunk=8192; free=chunk-48-len(help_entries); aoll=struct.pack('<4sI4Q2I',b'AOLL',free,0,0xffffffffffffffff,0xffffffffffffffff,0,1,0)+help_entries+bytes(free-2)+struct.pack('<H',2)
directory=struct.pack('<4s7I',b'IFCM',1,chunk,0x100000,0xffffffff,0xffffffff,1,0)+aoll; content_offset=0x178+len(directory)
post=struct.pack('<II',2,0x98)+struct.pack('<4Q4I2Q',0xffffffffffffffff,0,0,0,chunk,2,0,1,0,2)
post+=struct.pack('<4Q4I2Q',0xffffffffffffffff,0xffffffffffffffff,0xffffffffffffffff,0,512,2,0,0,0,0)+struct.pack('<IIQ',0x100000,0x20000,0)
post+=struct.pack('<4sIIHH8I',b'CAOL',2,0x50,0x4848,0,0,chunk,512,0x100000,0x20000,0,0,0)+struct.pack('<4sIIIQII',b'ITSF',4,32,1,content_offset,0,0x409)
header=struct.pack('<8s4I16s',b'ITOLITLS',1,0x28,5,len(post),uuid.UUID('0a9007c1-4076-11d3-8789-0000f8105754').bytes_le)+struct.pack('<10Q',0x160,24,0x178,len(directory),0,0,0,0,0,0)+post
assert len(header)==0x160 and len(post)==0xe8
aliases('Hxs',header+struct.pack('<IIQII',0x1fe,0,content_offset+len(name_list)+len(payload),0,0)+directory+name_list+payload,[payload])
# Existing Wine test data, compressed by Microsoft's COMPRESS.EXE. Correct the
# declared uncompressed size only; retain the compressed bytes and attribution.
szdd=bytes.fromhex('535a444488f02733417414000000df5468697320f2f06120ff746573742066696c03652e')
aliases('MsLZ',szdd,[b'This is a test file.'],provenance='Wine lz32 lzexpand_main.c compressed_file; header size corrected; licenses/wine-szdd-fixture.txt')
# Genuine PKZIP multi-volume data generated by the macOS zip tool.
def split_zip():
    large=payload*4096; (work/'split-payload.txt').write_bytes(large); path=root/'source-split.zip'
    command(['/usr/bin/zip','-q','-0','-s','64k',path,'split-payload.txt'])
    first=record('zip','z01',path.with_suffix('.z01'),[large])
    for part in root.glob('source-split.z*'):
        if part.suffix!='.z01':shutil.copyfile(part,first.with_suffix(part.suffix))
attempt('zip',split_zip)
def independent_archive_payloads(paths):
    # An independent system libarchive decoder supplies reference bytes for
    # third-party multi-volume fixtures; 7-Zip extraction is never its own oracle.
    library=ctypes.CDLL('/usr/lib/libarchive.2.dylib')
    library.archive_read_new.restype=ctypes.c_void_p
    for name in ['archive_read_support_filter_all','archive_read_support_format_all','archive_read_free']:
        getattr(library,name).argtypes=[ctypes.c_void_p]
    library.archive_read_open_filenames.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_char_p),ctypes.c_size_t]
    library.archive_read_next_header.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
    library.archive_read_data.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t]; library.archive_read_data.restype=ctypes.c_ssize_t
    library.archive_entry_filetype.argtypes=[ctypes.c_void_p]; library.archive_entry_filetype.restype=ctypes.c_uint
    library.archive_error_string.argtypes=[ctypes.c_void_p]; library.archive_error_string.restype=ctypes.c_char_p
    archive=library.archive_read_new(); contents=[]
    try:
        library.archive_read_support_filter_all(archive); library.archive_read_support_format_all(archive)
        names=(ctypes.c_char_p*(len(paths)+1))(*[str(p).encode() for p in paths],None)
        if library.archive_read_open_filenames(archive,names,65536)!=0:raise RuntimeError(str(library.archive_error_string(archive)))
        entry=ctypes.c_void_p(); buffer=ctypes.create_string_buffer(65536)
        while True:
            status=library.archive_read_next_header(archive,ctypes.byref(entry))
            if status==1:break
            if status!=0:raise RuntimeError(str(library.archive_error_string(archive)))
            if library.archive_entry_filetype(entry)!=0o100000:continue
            chunks=[]
            while True:
                size=library.archive_read_data(archive,buffer,len(buffer))
                if size<0:raise RuntimeError(str(library.archive_error_string(archive)))
                if size==0:break
                chunks.append(buffer.raw[:size])
            contents.append(b''.join(chunks))
        return contents
    finally:library.archive_read_free(archive)
for fmt,pattern in [('Rar','test_rar_multivolume_single_file.part*.rar.uu'),('Rar5','test_read_format_rar5_multiarchive.part*.rar.uu')]:
    def rar_volumes(f=fmt,p=pattern):
        originals=sorted((repo/'tests/fixtures').glob(p)); paths=[]
        if not originals:raise RuntimeError('Missing licensed multi-volume fixtures')
        for n,source in enumerate(originals):
            path=root/(f+'-original-'+str(n)+'.rar'); path.write_bytes(uu_fixture(source.name)); paths.append(path)
        contents=independent_archive_payloads(paths)
        first=record(f,'r00',paths[0],contents,provenance='libarchive v3.8.2 multi-volume set; renamed .r00/.r01/...; reference decoded by system libarchive; licenses/libarchive-fixtures.txt')
        for n,path in enumerate(paths[1:],1):shutil.copyfile(path,first.with_suffix('.r'+str(n).zfill(2)))
    attempt(fmt,rar_volumes)
# Minimal FAT12 floppy containing one regular FILE.TXT, no mount or device I/O.
fat=bytearray(2880*512); fat[:11]=b'\xeb\x3c\x90MSDOS5.0'; struct.pack_into('<HBHBHHBHHHII',fat,11,512,1,1,2,224,2880,0xf0,9,18,2,0,0)
fat[38]=0x29; fat[43:54]=b'PORT TEST  '; fat[54:62]=b'FAT12   '; fat[510:512]=b'\x55\xaa'
for sector in [1,10]:fat[sector*512:sector*512+6]=b'\xf0\xff\xff\xff\x0f\x00'
directory=19*512; fat[directory:directory+11]=b'FILE    TXT'; fat[directory+11]=0x20
struct.pack_into('<HI',fat,directory+26,2,len(payload)); fat[33*512:33*512+len(payload)]=payload
aliases('FAT',fat,[payload])
# MBR wrapper with one FAT partition. The engine may automatically enter FAT.
mbr=bytearray(512)+fat; mbr[446:462]=struct.pack('<B3sB3sII',0,b'\x00\x01\x00',1,b'\xfe\xff\xff',1,2880); mbr[510:512]=b'\x55\xaa'
aliases('MBR',mbr,[payload])
# Android sparse image with one raw chunk, decoded to the FAT floppy above.
sparse=struct.pack('<IHHHHIIII',0xed26ff3a,1,0,28,12,512,2880,1,0)+struct.pack('<HHII',0xcac1,0,2880,12+len(fat))+fat
aliases('Sparse',sparse,[payload])
# Apple Partition Map wrapping the same FAT partition (not a disk device).
apm=bytearray(3*512)+fat; apm[:4]=struct.pack('>HH',0x4552,512); struct.pack_into('>I',apm,4,len(apm)//512)
for sector,start,count,name,kind in [(1,1,2,b'Apple',b'Apple_partition_map'),(2,3,2880,b'Fixture',b'DOS_FAT_12')]:
    offset=sector*512; struct.pack_into('>HHIII',apm,offset,0x504d,0,2,start,count); apm[offset+16:offset+16+len(name)]=name; apm[offset+48:offset+48+len(kind)]=kind
aliases('APM',apm,[bytes(fat)])
# GPT with standard header/partition CRCs and a protective MBR.
gpt=bytearray((2880+68)*512); gpt[34*512:34*512+len(fat)]=fat; gpt[510:512]=b'\x55\xaa'; gpt[446:462]=struct.pack('<B3sB3sII',0,b'\0\x01\0',0xee,b'\xfe\xff\xff',1,len(gpt)//512-1)
entries=bytearray(128*128); entries[:16]=bytes.fromhex('a2a0d0ebe5b9334487c068b6b72699c7'); entries[16:32]=b'PORTTESTPARTGUID'; struct.pack_into('<QQQ',entries,32,34,34+2880-1,0); entries[56:70]='Fixture'.encode('utf-16le'); gpt[1024:1024+len(entries)]=entries
header=bytearray(512); header[:8]=b'EFI PART'; struct.pack_into('<II',header,8,0x10000,92); struct.pack_into('<QQQQ',header,24,1,len(gpt)//512-1,34,len(gpt)//512-34); header[56:72]=b'PORTTESTDISKGUID'; struct.pack_into('<QIII',header,72,2,128,128,zlib.crc32(entries)); struct.pack_into('<I',header,16,zlib.crc32(header[:92])); gpt[512:1024]=header
backup_entries_offset=(len(gpt)//512-33)*512; gpt[backup_entries_offset:backup_entries_offset+len(entries)]=entries; struct.pack_into('<I',header,16,0); struct.pack_into('<QQ',header,24,len(gpt)//512-1,1); struct.pack_into('<Q',header,72,len(gpt)//512-33); struct.pack_into('<I',header,16,zlib.crc32(header[:92])); gpt[-512:]=header
aliases('GPT',gpt,[payload])
# A fixed VHD disk with a checksummed footer and the FAT filesystem.
footer=bytearray(512); footer[:8]=b'conectix'; struct.pack_into('>IIQ',footer,8,2,0x10000,0xffffffffffffffff); footer[28:32]=b'PORT'; struct.pack_into('>I',footer,32,0x10000); footer[36:40]=b'Mac '; struct.pack_into('>QQII',footer,40,len(fat),len(fat),(80<<16)|(2<<8)|18,2); footer[68:84]=b'PORTTESTVHDGUID0'; struct.pack_into('>I',footer,64,(~sum(footer))&0xffffffff)
aliases('VHD',fat+footer,[payload])
# VHDX 1.0 fixed allocation, two headers/regions and real BAT mappings.
def crc32c(data):
    crc=0xffffffff
    for byte in data:
        crc ^= byte
        for _ in range(8):crc=(crc>>1)^(0x82f63b78 if crc&1 else 0)
    return crc^0xffffffff
unit=1<<20; vhdx=bytearray(6*unit); vhdx[:8]=b'vhdxfile'; creator='7-Zip Mac Port test'.encode('utf-16le'); vhdx[8:8+len(creator)]=creator
for n in range(2):
    header=bytearray(4096); header[:4]=b'head'; struct.pack_into('<Q',header,8,n+1); header[16:32]=uuid.UUID('11223344-5566-7788-99aa-bbccddeeff00').bytes_le; header[32:48]=uuid.UUID('88776655-4433-2211-aabb-ccddeeff0011').bytes_le; struct.pack_into('<HHIQ',header,64,0,1,unit,unit); struct.pack_into('<I',header,4,crc32c(header)); vhdx[(n+1)*65536:(n+1)*65536+4096]=header
region=bytearray(65536); region[:4]=b'regi'; struct.pack_into('<I',region,8,2)
for n,guid,offset in [(0,'8b7ca206-4790-4b9a-b8fe-575f050f886e',2*unit),(1,'2dc27766-f623-4200-9d64-115e9bfd4a08',3*unit)]:
    start=16+n*32; region[start:start+16]=uuid.UUID(guid).bytes_le; struct.pack_into('<QII',region,start+16,offset,unit,1)
struct.pack_into('<I',region,4,crc32c(region)); vhdx[3*65536:4*65536]=region; vhdx[4*65536:5*65536]=region
meta=bytearray(unit); meta[:8]=b'metadata'; struct.pack_into('<H',meta,10,5)
meta_fields=[('caa16737-fa36-4d43-b3b6-33f0aa44e76b',struct.pack('<II',unit,1),4),('2fa54224-cd1b-4876-b211-5dbed83bf4b8',struct.pack('<Q',len(fat)),6),('beca12ab-b2e6-4523-93ef-c309e000c746',uuid.UUID('12345678-1234-4321-abcd-123456789abc').bytes_le,6),('8141bf1d-a96f-4709-ba47-f233a8faab5f',struct.pack('<I',512),6),('cda348c7-445d-4471-9cc9-e9885251c556',struct.pack('<I',512),6)]
for n,(guid,data,flags) in enumerate(meta_fields):
    start=32+n*32; offset=65536+n*16; meta[start:start+16]=uuid.UUID(guid).bytes_le; struct.pack_into('<IIII',meta,start+16,offset,len(data),flags,0); meta[offset:offset+len(data)]=data
vhdx[2*unit:3*unit]=meta; struct.pack_into('<QQ',vhdx,3*unit,4*unit|6,5*unit|6); vhdx[4*unit:4*unit+len(fat)]=fat; aliases('VHDX',vhdx,[payload])
# VDI 1.1 fixed blocks, with a real block map and padded allocated blocks.
vdi=bytearray(1024+2*(1<<20)); vdi_label=b'<<< Oracle VM VirtualBox Disk Image >>>\n'; vdi[:len(vdi_label)]=vdi_label; struct.pack_into('<III',vdi,64,0xbeda107f,0x10001,400); struct.pack_into('<I',vdi,76,2); struct.pack_into('<II',vdi,0x154,512,1024); struct.pack_into('<I',vdi,0x168,512); struct.pack_into('<Q',vdi,0x170,len(fat)); struct.pack_into('<I',vdi,0x178,1<<20); struct.pack_into('<II',vdi,0x180,2,2); struct.pack_into('<II',vdi,512,0,1); vdi[1024:1024+len(fat)]=fat
aliases('VDI',vdi,[payload])
# QCOW v1 and QCOW2 v2/v3 with uncompressed clusters and real mappings.
for version,ext in [(1,'qcow'),(2,'qcow2'),(3,'qcow2c')]:
    cluster=1<<16; count=(len(fat)+cluster-1)//cluster; start=3 if version==1 else 5; qcow=bytearray((start+count)*cluster); qcow[:4]=b'QFI\xfb'; struct.pack_into('>I',qcow,4,version); struct.pack_into('>Q',qcow,24,len(fat)); struct.pack_into('>Q',qcow,40,cluster)
    if version==1:qcow[32:34]=bytes([16,9])
    else:
        struct.pack_into('>I',qcow,20,16); struct.pack_into('>I',qcow,36,1); struct.pack_into('>QI',qcow,48,3*cluster,1); struct.pack_into('>Q',qcow,3*cluster,4*cluster)
        for i in range(start+count):struct.pack_into('>H',qcow,4*cluster+i*2,1)
        if version==3:struct.pack_into('>II',qcow,96,4,104)
    struct.pack_into('>Q',qcow,cluster,2*cluster)
    for i in range(count):struct.pack_into('>Q',qcow,2*cluster+i*8,(start+i)*cluster)
    qcow[start*cluster:start*cluster+len(fat)]=fat; record('QCOW',ext,qcow,[payload])
# VMDK hosted sparse extent: directory + grain table + stored FAT grains.
grain=65536; count=(len(fat)+grain-1)//grain; vmdk=bytearray((count+1)*grain); vmdk[:4]=b'KDMV'; struct.pack_into('<IIQQQQIQQQ',vmdk,4,1,1,2880,128,0,0,512,0,1,128); vmdk[73:77]=b'\x0a\x20\x0d\x0a'; struct.pack_into('<I',vmdk,512,2)
for i in range(count):struct.pack_into('<I',vmdk,1024+4*i,128*(i+1))
vmdk[grain:grain+len(fat)]=fat; aliases('VMDK',vmdk,[payload])
# CramFS v2 directory/inode layout with zlib-compressed original payload.
compressed=zlib.compress(payload); cram=bytearray(104+len(compressed)); struct.pack_into('<IIII',cram,0,0x28cd3d45,len(cram),1,0); cram[16:32]=b'Compressed ROMFS'; struct.pack_into('<IIII',cram,32,0,0,1,2); cram[48:60]=b'Port Fixture'; struct.pack_into('<III',cram,64,0x41ed,24,(76//4)<<6); struct.pack_into('<III',cram,76,0x81a4,len(payload),((100//4)<<6)|3); cram[88:100]=b'payload.txt\0'; struct.pack_into('<I',cram,100,len(cram)); cram[104:]=compressed; struct.pack_into('<I',cram,32,zlib.crc32(cram)); aliases('CramFS',cram,[payload])
# Android LP metadata: RAW extent, complete geometry and metadata backups.
lp_payload=payload.ljust(512,b'\0'); lp=bytearray(13824); geometry=bytearray(52)
struct.pack_into('<II',geometry,0,0x616c4467,52); struct.pack_into('<III',geometry,40,512,1,512); geometry[8:40]=hashlib.sha256(geometry).digest(); lp[4096:4148]=geometry; lp[8192:8244]=geometry
tables=bytearray(188); tables[:7]=b'payload'; struct.pack_into('<IIII',tables,36,0,0,1,0); struct.pack_into('<QIQI',tables,52,1,0,26,0); tables[76:83]=b'default'; struct.pack_into('<QIIQ',tables,124,26,512,0,len(lp)); tables[148:153]=b'super'
metadata=bytearray(128); struct.pack_into('<IHHI',metadata,0,0x414c5030,10,0,128); struct.pack_into('<I',metadata,44,len(tables)); metadata[48:80]=hashlib.sha256(tables).digest()
for offset,descriptor in [(80,(0,1,52)),(92,(52,1,24)),(104,(76,1,48)),(116,(124,1,64))]:struct.pack_into('<III',metadata,offset,*descriptor)
metadata[12:44]=hashlib.sha256(metadata).digest(); lp[12288:12604]=metadata+tables; lp[12800:13116]=metadata+tables; lp[13312:]=lp_payload; aliases('LP',lp,[lp_payload])
# UEFI Firmware Volume / capsule containing one checksummed RAW FFS file.
# Only the stored containers/checksums are written; no compression is invented.
fv=bytearray(b'\xff'*144); fv[:72]=b'\0'*72; fv[16:32]=uuid.UUID('8c8ce578-8a3d-4f1c-9935-896185c32dd3').bytes_le
struct.pack_into('<Q4sIHHHBBIIII',fv,32,len(fv),b'_FVH',0x800,72,0,0,0,2,1,len(fv),0,0)
struct.pack_into('<H',fv,50,(-sum(struct.unpack('<36H',fv[:72])))&0xffff)
ffs=bytearray(24); ffs[:16]=uuid.UUID('11223344-5566-7788-99aa-bbccddeeff00').bytes_le; ffs[17]=(-sum(payload))&255; ffs[18:20]=bytes([1,0x40]); ffs[20:23]=(24+len(payload)).to_bytes(3,'little'); ffs[23]=0xf8; ffs[16]=(-(sum(ffs)-ffs[17]-ffs[23]))&255
fv[72:96]=ffs; fv[96:96+len(payload)]=payload; aliases('UEFIf',fv,[payload])
capsule=uuid.UUID('539182b9-abb5-4391-b69a-e3a943f72fcc').bytes_le+struct.pack('<III',28,0,len(fv)+28)+fv; aliases('UEFIc',capsule,[payload])
# ARJ method 0 stores data; only header CRC/container layout is written here.
def arj_block(data): return b'\x60\xea'+struct.pack('<H',len(data))+data+struct.pack('<I',zlib.crc32(data))+b'\0\0'
main_header=bytearray(30); main_header[:7]=bytes([30,3,1,0,0,0,2])
file_header=bytearray(30); file_header[:7]=bytes([30,3,1,0,0,0,0]); struct.pack_into('<III',file_header,12,len(payload),len(payload),zlib.crc32(payload)); struct.pack_into('<H',file_header,26,0x20)
aliases('Arj',arj_block(main_header+b'fixture.arj\0\0')+arj_block(file_header+b'payload.txt\0\0')+payload+b'\x60\xea\0\0',[payload])
# Genuine multi-part generic split stream.
split_path=record('Split','001',payload[:16],[payload]); split_path.with_suffix('.002').write_bytes(payload[16:])
# OLE Compound File v3 with a 4096-byte Payload stream, using regular sectors.
ole_payload=payload.ljust(4096,b'\0'); ole=bytearray(512+10*512); ole[:8]=bytes.fromhex('d0cf11e0a1b11ae1')
struct.pack_into('<HHHH',ole,24,0x3e,3,0xfffe,9); struct.pack_into('<H',ole,32,6)
struct.pack_into('<IIIIIIIII',ole,40,0,1,8,0,4096,0xfffffffe,0,0xfffffffe,0)
struct.pack_into('<109I',ole,76,9,*([0xffffffff]*108)); ole[512:512+4096]=ole_payload
def directory_entry(name,typ,start,size,child=0xffffffff):
    entry=bytearray(128); encoded=(name+'\0').encode('utf-16le'); entry[:len(encoded)]=encoded
    struct.pack_into('<HBBIII',entry,64,len(encoded),typ,1,0xffffffff,0xffffffff,child); struct.pack_into('<IQ',entry,116,start,size); return entry
ole[512+8*512:512+8*512+256]=directory_entry('Root Entry',5,0xfffffffe,0,1)+directory_entry('Payload',2,0,4096)
struct.pack_into('<128I',ole,512+9*512,*list(range(1,8)),0xfffffffe,0xfffffffe,0xfffffffd,*([0xffffffff]*118))
aliases('Compound',ole,[ole_payload])
# Minimal PE image with one stored .data section. It is never executed.
pe=bytearray(1024); pe[:2]=b'MZ'; struct.pack_into('<I',pe,60,128); pe[128:132]=b'PE\0\0'
struct.pack_into('<HHIIIHH',pe,132,0x14c,1,0,0,0,224,0x102); struct.pack_into('<H',pe,152,0x10b)
struct.pack_into('<III',pe,152+28,0x400000,4096,512); struct.pack_into('<II',pe,152+56,8192,512); struct.pack_into('<H',pe,152+68,3); struct.pack_into('<I',pe,152+92,16)
pe[376:384]=b'.data\0\0\0'; struct.pack_into('<IIIIIIHHI',pe,384,len(payload),4096,512,512,0,0,0,0,0xc0000040); pe[512:]=payload.ljust(512,b'\0')
aliases('PE',pe,[payload])
# Terse Executable with one section and stripped-size adjustment of zero.
te=bytearray(512); te[:2]=b'VZ'; struct.pack_into('<HBBHIIQ',te,2,0x14c,1,3,40,0,0x1000,0x400000); te[40:48]=b'.data\0\0\0'; struct.pack_into('<IIIIIIHHI',te,48,len(payload),4096,512,512,0,0,0,0,0xc0000040)
aliases('TE',te+payload.ljust(512,b'\0'),[payload])
def object_fixture(fmt,target,extension):
    source=work/'payload.c'; source.write_text('const char payload[] = "format fixture\\n";\n')
    path=root/('source.'+extension); command(['/usr/bin/clang','-target',target,'-c',source,'-o',path]); aliases(fmt,path,[b'format fixture\n\0']); return path
attempt('COFF',lambda:object_fixture('COFF','x86_64-pc-windows-msvc','obj'))
attempt('ELF',lambda:object_fixture('ELF','x86_64-linux-gnu','elf'))
attempt('MachO',lambda:object_fixture('MachO','arm64-apple-macos15','macho'))
def mub_fixture():
    first=root/'source.macho'; second=root/'source-x86.macho'; command(['/usr/bin/clang','-target','x86_64-apple-macos15','-c',work/'payload.c','-o',second])
    path=root/'source.mub'; command(['/usr/bin/lipo','-create',first,second,'-output',path]); aliases('Mub',path,[first.read_bytes(),second.read_bytes()])
attempt('Mub',mub_fixture)
if args.sevenzip_source:
    def ppmd_fixture():
        tool=root/'ppmd-fixture'; source=args.sevenzip_source/'C'; command(['/usr/bin/clang','-O2','-I'+str(source),repo/'tests/ppmd-fixture.c',source/'Ppmd8.c',source/'Ppmd8Enc.c','-o',tool])
        path=root/'source.pmd'; command([tool,work/'payload.txt',path]); aliases('Ppmd',path,[payload])
    attempt('Ppmd',ppmd_fixture)
# Flash stored tag + zlib-compressed SWF. No Flash runtime is used.
tag=struct.pack('<HI',1,0)+payload; tag_header=struct.pack('<H',(87<<6)|len(tag)); body=b'\x08\0'+struct.pack('<HH',12<<8,1)+tag_header+tag+b'\0\0'
swf=b'FWS\x09'+struct.pack('<I',8+len(body))+body
aliases('SWF',swf,[tag]); aliases('SWFc',b'CWS'+swf[3:8]+zlib.compress(body),[swf])
# FLV with one PCM audio tag; the reader extracts the raw sample bytes.
audio=b'\x00'+payload; flv=b'FLV\x01\x04\0\0\0\x09\0\0\0\0'+b'\x08'+len(audio).to_bytes(3,'big')+b'\0'*7+audio+struct.pack('>I',11+len(audio)); aliases('FLV',flv,[payload])
if args.fixture_tools:
    tools=args.fixture_tools.resolve()
    def nsis_fixture():
        path=tools.parent/'nsis-3.12-setup.exe'; source=tools.parent/'nsis-3.12-src.tar.bz2'
        with tarfile.open(source) as archive:license_text=archive.extractfile('nsis-3.12-src/COPYING').read()
        aliases('Nsis',path,[license_text],provenance='official NSIS 3.12 setup.exe; COPYING compared with independently read source tarball; never execute Windows installer')
    attempt('Nsis',nsis_fixture)
    def squashfs_fixture():
        path=root/'source.squashfs'; command([tools/'mksquashfs',work,path,'-noappend','-no-progress','-processors','2','-all-root']); aliases('SquashFS',path,[payload])
    attempt('SquashFS',squashfs_fixture)
    def ext_fixture():
        for fs in ['ext2','ext3','ext4']:
            path=root/('source.'+fs); command([tools/'mke2fs','-q','-F','-t',fs,'-b','1024','-d',work,path,'16384']); aliases('Ext',path,[payload],extensions=[fs])
            if fs=='ext4':aliases('Ext',path,[payload],extensions=['ext','img'])
    attempt('Ext',ext_fixture)
    def ntfs_fixture():
        path=root/'source.ntfs'
        with path.open('wb') as stream:stream.truncate(64*1024*1024)
        command([tools/'mkntfs','-F','-Q','-s','512',path]); command([tools/'ntfscp','-f',path,work/'payload.txt','/payload.txt']); aliases('NTFS',path,[payload])
    attempt('NTFS',ntfs_fixture)
def hybrid(fmt,flag):
    path=root/('source-'+fmt+'.iso'); command(['/usr/bin/hdiutil','makehybrid',flag,'-o',path,work,'-quiet']); aliases(fmt,path,[payload])
attempt('Iso',lambda:hybrid('Iso','-iso')); attempt('Udf',lambda:hybrid('Udf','-udf'))
for fmt,filesystem,extra in [('APFS','APFS',[]),('HFS','HFS+',[]),('Dmg','HFS+',[])]:
    def disk(f=fmt,fs=filesystem):
        path=root/('source-'+f+'.dmg'); image_format='UDZO' if f=='Dmg' else 'UFBI'
        command(['/usr/bin/hdiutil','create','-srcfolder',work,'-fs',fs,'-layout','NONE','-format',image_format,'-volname','Fixture','-nospotlight','-quiet',path])
        aliases(f,path,[payload])
    attempt(fmt,disk)
covered={(c['format'],c['extension']) for c in cases}
pending=[{'format':f['name'],'extension':e} for f in inventory['formats'] for e in f['extensions'] if (f['name'],e) not in covered]
(root/'manifest.json').write_text(json.dumps({'version':inventory['version'],'cases':cases,'pending':pending,'generationFailures':failures},ensure_ascii=False,indent=2)+'\n')
print(f'Created {len(cases)} genuine-format alias fixtures; {len(pending)} registration cases pending. Manifest: {root}/manifest.json',flush=True)
